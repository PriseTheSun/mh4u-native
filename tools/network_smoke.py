#!/usr/bin/env python3
"""Exercise the public native CLI with separate server/client processes and malformed peers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import socket
import struct
import subprocess
import tempfile
import time
import zlib

from network_manifest import generate


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def packet(kind, payload=b"", version=1, length=None):
    return struct.pack("!4sHBBI", b"M4NL", version, kind, 0, len(payload) if length is None else length) + payload


def exact(peer, size):
    data = b""
    while len(data) < size:
        block = peer.recv(size - len(data))
        if not block:
            raise EOFError("peer closed")
        data += block
    return data


def receive(peer):
    magic, version, kind, flags, length = struct.unpack("!4sHBBI", exact(peer, 12))
    require((magic, version, flags) == (b"M4NL", 1, 0) and length <= 512, "invalid server frame")
    return kind, exact(peer, length)


def until(peer, expected):
    for _ in range(140):
        kind, body = receive(peer)
        if kind == expected:
            return body
    raise AssertionError("expected frame missing")


def fixture(directory):
    directory.mkdir()
    (directory / "mesh.bin").write_bytes(b"\0\0\0\0")
    def chunk(kind, payload):
        return struct.pack("!I", len(payload)) + kind + payload + struct.pack("!I", zlib.crc32(kind + payload))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack("!IIBBBBB", 1, 1, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(b"\0\xff\xff\xff")) + chunk(b"IEND", b"")
    (directory / "pixel.png").write_bytes(png)
    scene = {"asset": {"version": "2.0"}, "buffers": [{"uri": "mesh.bin", "byteLength": 4}], "images": [{"uri": "pixel.png"}], "animations": [{"name": "diagnostic_clip_0", "samplers": [], "channels": []}, {"name": "diagnostic_clip_1", "samplers": [], "channels": []}]}
    path = directory / "scene.gltf"
    path.write_text(json.dumps(scene), encoding="utf-8")
    return path


def run(binary, root, scene=None):
    children = []
    streams = []
    sockets = []
    checks = []
    def launch(name, arguments):
        path = root / (name + ".log")
        stream = path.open("w", encoding="utf-8")
        streams.append(stream)
        child = subprocess.Popen([str(binary), *arguments], stdout=stream, stderr=subprocess.STDOUT)
        children.append(child)
        return child, path
    def text(path):
        return path.read_text(encoding="utf-8")
    def wait_text(path, pattern, timeout=3):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            found = re.search(pattern, text(path))
            if found:
                return found
            time.sleep(0.02)
        raise AssertionError(f"Missing {pattern!r} in {path.name}: {text(path)[-1500:]}")
    def connect(port):
        peer = socket.create_connection(("127.0.0.1", port), timeout=2)
        peer.settimeout(2)
        sockets.append(peer)
        return peer
    try:
        if scene:
            shutil.copytree(scene.parent, root / "content")
            scene = root / "content" / scene.name
        else:
            scene = fixture(root / "content")
        manifest = scene.parent / "network-smoke.manifest"
        # A copied prior test manifest is owned test data and may be removed.
        if manifest.exists():
            manifest.unlink()
        digest = generate(scene, manifest)
        arguments = ["--manifest", str(manifest)]
        verified = subprocess.run([str(binary), "verify", *arguments], text=True, capture_output=True, timeout=5)
        require(verified.returncode == 0 and f"verified={digest}" in verified.stdout, "native/Python manifest SHA256 mismatch: " + verified.stdout + verified.stderr)
        checks.append("verified content manifest matches Python SHA256")
        document = manifest.read_text(encoding="utf-8")
        binary_record = next(line for line in document.splitlines() if line.startswith("file\t") and line.endswith(".bin"))
        resource = manifest.parent / binary_record.split("\t")[3]
        original = resource.read_bytes()
        resource.write_bytes(bytes([original[0] ^ 1]) + original[1:])
        tampered = subprocess.run([str(binary), "verify", *arguments], text=True, capture_output=True, timeout=5)
        require(tampered.returncode != 0 and "SHA256 mismatch" in tampered.stderr, "native verifier accepted modified buffer")
        resource.write_bytes(original)
        lines = document.splitlines()
        manifest.write_text("\n".join(line for line in lines if line != binary_record) + "\n", encoding="utf-8")
        omitted = subprocess.run([str(binary), "verify", *arguments], text=True, capture_output=True, timeout=5)
        require(omitted.returncode != 0 and ("exactly cover" in omitted.stderr or "missing glTF" in omitted.stderr), "manifest accepted omitted dependency")
        manifest.write_text(document.replace("clip\t0\t", "clip\t0\twrong_", 1), encoding="utf-8")
        reordered = subprocess.run([str(binary), "verify", *arguments], text=True, capture_output=True, timeout=5)
        require(reordered.returncode != 0 and "order/name" in reordered.stderr, "manifest accepted modified clip order/name")
        manifest.write_text(document, encoding="utf-8")
        checks.append("buffer tampering, omitted dependency and clip identity rejected")
        server, server_log = launch("server", ["server", *arguments, "--port", "0", "--run-ms", "11000", "--handshake-ms", "300", "--idle-ms", "700"])
        port = int(wait_text(server_log, r"READY port=(\d+)").group(1))
        def hello(name, content=digest):
            encoded = name.encode("ascii")
            return packet(1, bytes.fromhex(content) + bytes([len(encoded)]) + encoded)
        def joined(name):
            peer = connect(port)
            peer.sendall(hello(name))
            until(peer, 2)
            return peer
        first, first_log = launch("client-one", ["client", *arguments, "--port", str(port), "--name", "one", "--select", "0", "--select-after-ms", "100", "--run-ms", "1600"])
        second, second_log = launch("client-two", ["client", *arguments, "--port", str(port), "--name", "two", "--select", "1", "--select-after-ms", "300", "--run-ms", "1900"])
        for log in (first_log, second_log):
            wait_text(log, r"STATE .*selected=1 members=.*one.*two|STATE .*selected=1 members=.*two.*one")
        extra_a, extra_b = joined("three"), joined("four")
        fifth = connect(port)
        fifth.sendall(hello("five"))
        reason = until(fifth, 8)
        require(reason[1:] == b"session_full", "fifth participant was accepted")
        for peer in (extra_a, extra_b, fifth):
            peer.close()
        require(first.wait(timeout=4) == 0 and second.wait(timeout=4) == 0, "native clients failed")
        for log in (first_log, second_log):
            require("confirmed=1 sockets=0" in text(log), "native client cleanup not confirmed")
        intents = re.findall(r"INTENT revision=(\d+) id=(\d+) sequence=(\d+) selected=(\d+)", text(server_log))
        require(len(intents) == 2 and int(intents[0][0]) < int(intents[1][0]) and [v[3] for v in intents] == ["0", "1"], "server did not authoritatively order client intents")
        checks.append("separate native server + two native clients agree on ordered state and clean disconnect")
        checks.append("four-participant capacity enforced")
        if os.name != "nt":
            for stop_signal in (signal.SIGINT, signal.SIGTERM):
                label = "signal-" + stop_signal.name.lower()
                signaled, signaled_log = launch(label, ["client", *arguments, "--port", str(port), "--name", label, "--run-ms", "15000"])
                participant = wait_text(signaled_log, r"WELCOME id=(\d+)").group(1)
                signaled.send_signal(stop_signal)
                require(signaled.wait(timeout=2) == 0 and "confirmed=1 sockets=0" in text(signaled_log), "signaled client did not complete Bye: " + label)
                wait_text(server_log, rf"LEAVE id={participant} reason=client_bye")
            checks.append("SIGINT and SIGTERM initiate bounded Bye exchange and server-observed departure")
        cases = [
            ("version", packet(1, version=2), b"protocol_version"),
            ("length", packet(1, length=0xffffffff), b"payload_limit"),
            ("content", hello("mismatch", "00" * 32), b"content_mismatch"),
            ("prehello", packet(4, struct.pack("!QH", 1, 0)), b"hello_required"),
        ]
        for label, malformed, expected in cases:
            peer = connect(port)
            peer.sendall(malformed)
            reason = until(peer, 8)
            require(reason[1:] == expected, "bad rejection for " + label)
            peer.close()
        peer = joined("rate")
        peer.sendall(b"".join(packet(5, struct.pack("!Q", index)) for index in range(1, 131)))
        require(until(peer, 8)[1:] == b"message_rate_limit", "message rate limit not enforced")
        peer.close()
        peer = connect(port)
        require(until(peer, 8)[1:] == b"handshake_timeout", "handshake deadline not enforced")
        peer.close()
        peer = joined("quiet")
        require(until(peer, 8)[1:] == b"idle_timeout", "idle deadline not enforced")
        peer.close()
        peer = joined("partial")
        peer.sendall(packet(4, b"\0", length=10))
        require(until(peer, 8)[1:] == b"partial_frame_timeout", "partial-frame deadline not enforced")
        peer.close()
        for name, payload, expected in [("sequence", struct.pack("!QH", 2, 0), b"intent_sequence"), ("selection", struct.pack("!QH", 1, 65000), b"invalid_selection")]:
            peer = joined(name)
            peer.sendall(packet(4, payload))
            require(until(peer, 8)[1:] == expected, "invalid intent accepted: " + name)
            peer.close()
        checks.append("version/length/content/pre-handshake/rate/sequence/selection violations rejected")
        checks.append("handshake, idle and partial-frame deadlines enforced")
        peer = connect(port)
        message = hello("fragmented")
        for offset in range(0, len(message), 7):
            peer.sendall(message[offset:offset + 7])
            time.sleep(0.01)
        until(peer, 2)
        peer.sendall(packet(4, struct.pack("!QH", 1, 0)) + packet(4, struct.pack("!QH", 2, 1)) + packet(5, struct.pack("!Q", 42)))
        require(until(peer, 6) == struct.pack("!Q", 42), "fragmented/coalesced frames failed")
        peer.close()
        checks.append("fragmented handshake and coalesced intents/heartbeat handled")
        time.sleep(0.15)
        slow = [joined("slow_reader_" + str(i).zfill(12)) for i in range(4)]
        slow[0].sendall(b"".join(packet(4, struct.pack("!QH", sequence, sequence % 2)) for sequence in range(1, 129)))
        wait_text(server_log, r"reason=outbound_queue_limit", 2)
        for peer in slow:
            peer.close()
        checks.append("stalled readers/burst hit bounded outbound queue without blocking server")
        time.sleep(0.15)
        survivor, survivor_log = launch("client-after-malformed", ["client", *arguments, "--port", str(port), "--name", "survivor", "--select", "0", "--run-ms", "15000"])
        wait_text(survivor_log, r"WELCOME")
        require(server.wait(timeout=12) == 0, "server failed shutdown")
        require(survivor.wait(timeout=3) == 0 and "confirmed=1 sockets=0" in text(survivor_log), "server shutdown not received by live client")
        require("STOP peers=0 sockets=0" in text(server_log), "server cleanup incomplete")
        # TIME_WAIT may prevent immediate rebinding; refusal to connect establishes no listener remains.
        try:
            probe = socket.create_connection(("127.0.0.1", port), timeout=0.2)
        except OSError:
            pass
        else:
            probe.close()
            raise AssertionError("listener still alive after server exit")
        checks.append("server remains usable after malformed traffic; live client and listener clean up on shutdown")
        report = {"status": "passed", "transport": "TCP loopback", "binary": str(binary), "content_id": digest, "process_ids": [child.pid for child in children], "checks": checks}
        (root / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(report, indent=2))
    finally:
        for peer in sockets:
            peer.close()
        for child in children:
            if child.poll() is None:
                child.terminate()
        for child in children:
            try:
                child.wait(timeout=2)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait(timeout=2)
        for stream in streams:
            stream.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--scene", type=Path)
    args = parser.parse_args()
    if args.output:
        args.output.mkdir(parents=True, exist_ok=False)
        run(args.binary.resolve(), args.output.resolve(), args.scene.resolve() if args.scene else None)
    else:
        with tempfile.TemporaryDirectory(prefix="mh4u-network-smoke-") as temporary:
            run(args.binary.resolve(), Path(temporary), args.scene.resolve() if args.scene else None)


if __name__ == "__main__":
    main()
