#!/usr/bin/env python3
"""Exercise the native reusable world client's server-message boundary."""
import argparse
import json
import os
from pathlib import Path
import signal
import socket
import struct
import subprocess
import tempfile
import time
from network_world_identity import identify
from network_world_smoke import packet, receive, require


def run(binary, world, output):
    identity = identify(world)
    digest = bytes.fromhex(identity['digest'])
    clip_count = len(identity['clips'])
    checks = []
    children = []
    def state(revision=1, tick=1, session=123, identity_digest=digest, x=0.0, flags=0, accepted=0, count=1):
        header = struct.pack('!Q32sQQHB', session, identity_digest, tick, revision, 65535, count)
        member = struct.pack('!IB', 1, 8)+b'consumer'+struct.pack('!QQQddddB', 0, accepted, 0, x, 0, 0, 0, flags)
        return packet(3, header+(member if count else b''))
    welcome = packet(2, struct.pack('!IHQ32sHH', 1, clip_count, 123, digest, 50, 250))
    cases = [
        ('digest', welcome+state(identity_digest=b'\0'*32), 'world_state_identity'),
        ('session', welcome+state(session=124), 'world_state_identity'),
        ('revision', welcome+state()+state(), 'world_state_order'),
        ('tick', welcome+state(tick=2)+state(revision=2, tick=1), 'world_state_order'),
        ('nan', welcome+state(x=float('nan')), 'nonfinite_wire_number'),
        ('flags', welcome+state(flags=4), 'invalid_world_participant'),
        ('missing', welcome+state(count=0), 'self_missing_from_state'),
        ('ack', welcome+state(accepted=1), 'state_identity'),
        ('version', packet(2, version=1), 'protocol_version'),
        ('length', packet(2, length=513), 'payload_limit'),
        ('partial', welcome+packet(3, b'\0', length=59), 'partial_frame_timeout'),
        ('silent', b'', 'handshake_timeout'),
    ]
    for label, payload, expected in cases:
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0))
            listener.listen(1)
            listener.settimeout(3)
            with (output/(label+'.log')).open('w') as stream:
                child = subprocess.Popen([str(binary), 'client', '--world', str(world), '--port', str(listener.getsockname()[1]), '--name', 'consumer', '--handshake-ms', '250'], stdout=stream, stderr=subprocess.STDOUT)
                children.append(child)
                try:
                    with listener.accept()[0] as peer:
                        peer.settimeout(3)
                        require(receive(peer)[0] == 1, 'native Hello missing')
                        if payload:
                            peer.sendall(payload)
                        require(child.wait(timeout=3) == 2, 'malformed server did not cause bounded client failure: '+label)
                finally:
                    if child.poll() is None:
                        child.terminate()
                        try:
                            child.wait(timeout=2)
                        except subprocess.TimeoutExpired:
                            child.kill()
                            child.wait(timeout=2)
            require(expected in (output/(label+'.log')).read_text(), 'wrong native client rejection '+label)
            checks.append(label+': '+expected)
    if os.name != 'nt':
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0));listener.listen(1);listener.settimeout(3)
            with (output/'signal-before-welcome.log').open('w') as stream:
                child = subprocess.Popen([str(binary), 'client', '--world', str(world), '--port', str(listener.getsockname()[1]), '--name', 'consumer'], stdout=stream, stderr=subprocess.STDOUT)
                children.append(child)
                try:
                    with listener.accept()[0] as peer:
                        peer.settimeout(2)
                        require(receive(peer)[0] == 1, 'Hello missing before cancellation')
                        child.send_signal(signal.SIGTERM)
                        time.sleep(0.1)
                        peer.sendall(welcome)
                        require(receive(peer)[0] == 7, 'canceled handshake did not transition to Bye')
                        peer.sendall(packet(7))
                        require(child.wait(timeout=2) == 0, 'canceled handshake did not confirm Bye')
                finally:
                    if child.poll() is None:
                        child.terminate()
                        try:
                            child.wait(timeout=2)
                        except subprocess.TimeoutExpired:
                            child.kill();child.wait(timeout=2)
            require('confirmed=1 sockets=0' in (output/'signal-before-welcome.log').read_text(), 'canceled handshake cleanup missing')
            checks.append('SIGTERM while waiting for Welcome completes bounded Bye after late handshake')
    report = {'status': 'passed', 'world_digest': identity['digest'], 'binary': str(binary), 'process_ids': [child.pid for child in children], 'checks': checks}
    (output/'validation.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--world', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.output:
        args.output.mkdir(parents=True, exist_ok=False)
        run(args.binary.resolve(), args.world.resolve(), args.output.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix='mh4u-world-client-') as directory:
            run(args.binary.resolve(), args.world.resolve(), Path(directory))


if __name__ == '__main__':
    main()
