#!/usr/bin/env python3
"""Real v1/v2 servers: accept order must not determine participant wire order."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import socket
import struct
import subprocess
import tempfile
import time

from network_manifest import generate
from network_smoke import fixture, packet
from network_world_identity import identify


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def exact(peer, size):
    data = bytearray()
    while len(data) < size:
        block = peer.recv(size-len(data))
        if not block:
            raise EOFError('peer closed before expected frame')
        data.extend(block)
    return bytes(data)


def receive(peer, version):
    magic, actual_version, kind, flags, length = struct.unpack('!4sHBBI', exact(peer, 12))
    require((magic, actual_version, flags) == (b'M4NL', version, 0) and length <= 512,
            'invalid server envelope')
    return kind, exact(peer, length)


def until(peer, version, expected, states=None):
    for _ in range(100):
        kind, body = receive(peer, version)
        if kind == 8:
            raise AssertionError('server rejected participant: '+body[1:].decode('ascii', errors='replace'))
        if kind == 7 and expected != 7:
            raise AssertionError('server closed session before expected message')
        if kind == 3 and states is not None:
            states.append(body)
        if kind == expected:
            return body
    raise AssertionError('expected message absent within frame budget')


def welcome(body, version, digest):
    require(len(body) == (46 if version == 1 else 50), 'invalid welcome length')
    participant, clips, session, actual_digest = struct.unpack_from('!IHQ32s', body)
    require(participant > 0 and actual_digest.hex() == digest, 'welcome identity differs')
    if version == 2:
        require(struct.unpack_from('!HH', body, 46) == (50, 250), 'world timing contract differs')
    return {'id': participant, 'clips': clips, 'session': session, 'digest': actual_digest.hex()}


def membership(body, version, identity):
    if version == 1:
        require(len(body) >= 11, 'truncated session state')
        revision, selection, count = struct.unpack_from('!QHB', body)
        offset = 11
    else:
        require(len(body) >= 59, 'truncated world state')
        session, digest, tick, revision, selection, count = struct.unpack_from('!Q32sQQHB', body)
        require(session == identity['session'] and digest.hex() == identity['digest'],
                'world snapshot identity differs')
        offset = 59
    require(revision > 0 and count <= 4, 'invalid snapshot revision/count')
    members = []
    for _ in range(count):
        require(offset+5 <= len(body), 'truncated participant')
        participant, size = struct.unpack_from('!IB', body, offset)
        offset += 5
        require(offset+size <= len(body), 'truncated participant name')
        name = body[offset:offset+size].decode('ascii')
        offset += size
        # Observe membership order without reinterpreting movement or animation.
        tail = 8 if version == 1 else 57
        require(offset+tail <= len(body), 'truncated participant fields')
        offset += tail
        members.append({'id': participant, 'name': name})
    require(offset == len(body), 'trailing snapshot data')
    return {'revision': revision, 'members': members}


def wait_state(peer, version, identity, count, observed):
    for _ in range(100):
        kind, body = receive(peer, version)
        if kind == 8:
            raise AssertionError('server rejected participant: '+body[1:].decode('ascii', errors='replace'))
        require(kind != 7, 'server closed before membership snapshot')
        if kind != 3:
            continue
        state = membership(body, version, identity)
        observed.append(state)
        ids = [member['id'] for member in state['members']]
        require(ids == sorted(set(ids)) and all(ids), f'noncanonical participant order {ids}')
        if len(ids) == count:
            return state
    raise AssertionError('membership snapshot absent within frame budget')


def run_case(binary, world, output, version):
    output.mkdir()
    if version == 1:
        scene = fixture(output/'content')
        manifest = scene.parent/'network-membership.manifest'
        digest = generate(scene, manifest)
        content_arguments = ['--manifest', str(manifest)]
    else:
        digest = identify(world)['digest']
        content_arguments = ['--world', str(world)]
    log = output/'server.log'
    stream = log.open('w', encoding='utf-8')
    command = [str(binary), 'server', *content_arguments, '--port', '0', '--run-ms', '10000',
               '--handshake-ms', '3000', '--idle-ms', '3000']
    child = None
    peers = []
    report = {'protocol': version, 'command': command, 'content_digest': digest,
              'welcomes': {}, 'snapshots': {'A': [], 'B': []}, 'errors': []}
    try:
        child = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT)
        deadline = time.monotonic()+12
        ready = None
        while time.monotonic() < deadline:
            ready = re.search(r'READY port=(\d+) protocol=(\d+) content=([0-9a-f]+)', log.read_text())
            if ready or child.poll() is not None:
                break
            time.sleep(0.01)
        require(ready is not None, 'server failed to become ready: '+log.read_text()[-1000:])
        require(int(ready.group(2)) == version and ready.group(3) == digest, 'server content/version differs')
        port = int(ready.group(1))

        def connect():
            peer = socket.create_connection(('127.0.0.1', port), timeout=2)
            peer.settimeout(2)
            peers.append(peer)
            return peer

        def hello(peer, name):
            encoded = name.encode('ascii')
            peer.sendall(packet(1, bytes.fromhex(digest)+bytes([len(encoded)])+encoded, version=version))

        # Connect completion precedes the second connect. A sends no HELLO until
        # B's WELCOME proves the server has assigned B an ID. No timing sleep.
        a = connect()
        b = connect()
        hello(b, 'early-hello')
        report['welcomes']['B'] = welcome(until(b, version, 2), version, digest)
        require(report['welcomes']['B']['id'] == 1, 'B must receive first assigned ID')
        hello(a, 'late-hello')
        report['welcomes']['A'] = welcome(until(a, version, 2), version, digest)
        require(report['welcomes']['A']['id'] == 2, 'A must receive second assigned ID')
        require(report['welcomes']['A']['session'] == report['welcomes']['B']['session'], 'session changed between welcomes')
        expected = [{'id': 1, 'name': 'early-hello'}, {'id': 2, 'name': 'late-hello'}]
        # Inspect both recipients even when the first exposes the regression.
        for label, peer in [('A', a), ('B', b)]:
            try:
                state = wait_state(peer, version, report['welcomes'][label], 2, report['snapshots'][label])
                require(state['members'] == expected, 'welcome/snapshot participant identity differs')
            except (AssertionError, OSError, EOFError, ValueError) as error:
                report['errors'].append(f'{label}: {error}')
        if not report['errors']:
            a.sendall(packet(7, version=version))
            require(until(a, version, 7) == b'', 'A Bye not acknowledged')
            a.close()
            remaining = wait_state(b, version, report['welcomes']['B'], 1, report['snapshots']['B'])
            require(remaining['members'] == expected[:1], 'departure changed surviving identity')
            b.sendall(packet(7, version=version))
            require(until(b, version, 7) == b'', 'B Bye not acknowledged')
            b.close()
    except (AssertionError, OSError, EOFError, ValueError, subprocess.SubprocessError) as error:
        report['errors'].append(str(error))
    finally:
        for peer in peers:
            peer.close()
        if child is not None:
            if child.poll() is None:
                child.terminate()
            try:
                report['server_exit_code'] = child.wait(timeout=3)
            except subprocess.TimeoutExpired:
                child.kill()
                report['server_exit_code'] = child.wait(timeout=3)
                report['errors'].append('owned server required kill after cleanup deadline')
        stream.close()
        report['open_sockets_after_cleanup'] = sum(peer.fileno() != -1 for peer in peers)
        report['owned_process_running_after_cleanup'] = child is not None and child.poll() is None
        server_text = log.read_text()
        report['server_clean_stop'] = 'STOP peers=0 sockets=0' in server_text
        if report.get('server_exit_code') != 0 or not report['server_clean_stop']:
            report['errors'].append('server did not report clean shutdown')
        report['status'] = 'passed' if not report['errors'] else 'failed'
        (output/'result.json').write_text(json.dumps(report, indent=2)+'\n')
    return report


def run(binary, world, output):
    before = hashlib.sha256(binary.read_bytes()).hexdigest()
    cases = []
    for version in (1, 2):
        try:
            cases.append(run_case(binary, world, output/f'v{version}', version))
        except Exception as error:
            cases.append({'protocol': version, 'status': 'failed', 'errors': [f'case setup: {error}']})
    require(hashlib.sha256(binary.read_bytes()).hexdigest() == before, 'binary changed during regression')
    report = {'schema': 'mh4u-network-membership-order-v1', 'binary': str(binary), 'binary_sha256': before,
              'status': 'passed' if all(case['status'] == 'passed' for case in cases) else 'failed', 'cases': cases}
    (output/'validation.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({'status': report['status'], 'binary_sha256': before,
                      'cases': [{'protocol': c['protocol'], 'status': c['status'], 'errors': c['errors']} for c in cases]}, indent=2))
    return 0 if report['status'] == 'passed' else 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--world', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    binary, world = args.binary.resolve(strict=True), args.world.resolve(strict=True)
    if args.output is None:
        with tempfile.TemporaryDirectory(prefix='mh4u-membership-') as temporary:
            return run(binary, world, Path(temporary))
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    return run(binary, world, output)


if __name__ == '__main__':
    raise SystemExit(main())
