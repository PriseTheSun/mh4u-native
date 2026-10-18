#!/usr/bin/env python3
"""Public v2 server + native clients over verified real diagnostic world data."""
import argparse
import json
import math
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
from network_world_identity import identify


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def packet(kind, body=b'', version=2, length=None):
    return struct.pack('!4sHBBI', b'M4NL', version, kind, 0, len(body) if length is None else length)+body


def exact(peer, length):
    data = b''
    while len(data) < length:
        block = peer.recv(length-len(data))
        if not block:
            raise EOFError('peer closed')
        data += block
    return data


def receive(peer):
    magic, version, kind, flags, size = struct.unpack('!4sHBBI', exact(peer, 12))
    require((magic, version, flags) == (b'M4NL', 2, 0) and size <= 512, 'invalid server envelope')
    return kind, exact(peer, size)


def until(peer, kind):
    for _ in range(160):
        current, body = receive(peer)
        if current == kind:
            return body
    raise AssertionError('missing expected message')


def decode_state(data):
    session, digest, tick, revision, selection, count = struct.unpack_from('!Q32sQQHB', data)
    offset = 59
    members = {}
    for _ in range(count):
        participant, length = struct.unpack_from('!IB', data, offset)
        offset += 5
        name = data[offset:offset+length].decode('ascii')
        offset += length
        selection_ack, accepted, applied, x, y, z, yaw, flags = struct.unpack_from('!QQQddddB', data, offset)
        offset += 57
        members[name] = {'id': participant, 'accepted': accepted, 'applied': applied,
                         'x': x, 'y': y, 'z': z, 'yaw': yaw, 'blocked': bool(flags & 1),
                         'expired': bool(flags & 2), 'selection_ack': selection_ack}
    require(offset == len(data) and count <= 4, 'malformed world snapshot')
    return {'session': session, 'digest': digest.hex(), 'tick': tick, 'revision': revision, 'members': members, 'selection': selection}


def states(log):
    result = []
    for line in log.read_text(encoding='utf-8').splitlines():
        if not line.startswith('WORLD_STATE '):
            continue
        fields = dict(item.split('=', 1) for item in line.split()[1:])
        members = {}
        for encoded in filter(None, fields['members'].split(',')):
            participant, name, accepted, applied, x, y, z, yaw, blocked, expired, selected = encoded.split(':')
            members[name] = {'id': int(participant), 'accepted': int(accepted), 'applied': int(applied),
                             'x': float(x), 'y': float(y), 'z': float(z), 'yaw': float(yaw),
                             'blocked': blocked == '1', 'expired': expired == '1', 'selection_ack': int(selected)}
        result.append({'session': int(fields['session']), 'tick': int(fields['tick']), 'revision': int(fields['revision']),
                       'selection': int(fields['selected']), 'members': members})
    return result


def run(binary, recipe, output):
    source = identify(recipe)
    content = output/'content'
    content.mkdir()
    for relative in source['files']:
        destination = content/relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(recipe.parent/relative, destination)
    recipe = content/recipe.name
    identity = identify(recipe)
    (output/'world.identity').write_bytes(identity['document'])
    digest = identity['digest']
    children, streams, sockets, checks = [], [], [], []

    def launch(name, arguments):
        log = output/(name+'.log')
        stream = log.open('w', encoding='utf-8')
        streams.append(stream)
        child = subprocess.Popen([str(binary), *arguments], stdout=stream, stderr=subprocess.STDOUT)
        children.append(child)
        return child, log

    def wait_text(log, pattern, timeout=4):
        deadline = time.monotonic()+timeout
        while time.monotonic() < deadline:
            found = re.search(pattern, log.read_text(encoding='utf-8'))
            if found:
                return found
            time.sleep(0.02)
        raise AssertionError(f'Missing {pattern}: {log.read_text(encoding="utf-8")[-1600:]}')

    def connect(port):
        peer = socket.create_connection(('127.0.0.1', port), timeout=2)
        peer.settimeout(2)
        sockets.append(peer)
        return peer

    def hello(name, content_digest=digest):
        name = name.encode('ascii')
        return packet(1, bytes.fromhex(content_digest)+bytes([len(name)])+name)

    def finish(peer):
        peer.sendall(packet(7))
        require(until(peer, 7) == b'', 'Bye not acknowledged')
        peer.close()

    args = ['--world', str(recipe)]
    try:
        verified = subprocess.run([str(binary), 'verify', *args], capture_output=True, text=True, timeout=15)
        require(verified.returncode == 0 and f'WORLD verified={digest}' in verified.stdout, 'Python/native world digest differs: '+verified.stderr)
        (output/'verify.log').write_text(verified.stdout+verified.stderr)
        checks.append('native world identity equals independent Python inventory of scene, actor, collision, dependencies and ordered clips')
        server, server_log = launch('server', ['server', *args, '--port', '0', '--run-ms', '16000', '--handshake-ms', '350', '--idle-ms', '900'])
        port = int(wait_text(server_log, r'READY port=(\d+) protocol=2').group(1))

        def joined(name):
            peer = connect(port)
            peer.sendall(hello(name))
            welcome = until(peer, 2)
            require(len(welcome) == 50 and struct.unpack_from('!HH', welcome, 46) == (50, 250), 'world welcome contract differs')
            return peer

        one, one_log = launch('one', ['client', *args, '--port', str(port), '--name', 'one', '--move-x', '1000', '--select', '0', '--run-ms', '3500'])
        two, two_log = launch('two', ['client', *args, '--port', str(port), '--name', 'two', '--move-z', '1000', '--run-ms', '4300'])
        for log in (one_log, two_log):
            wait_text(log, r'WORLD_STATE .*members=.*one.*two|WORLD_STATE .*members=.*two.*one')
        third, fourth = joined('three'), joined('four')
        fifth = connect(port)
        fifth.sendall(hello('five'))
        require(until(fifth, 8)[1:] == b'session_full', 'world fifth member accepted')
        fifth.close()
        finish(third)
        finish(fourth)
        require(one.wait(timeout=6) == 0 and two.wait(timeout=3) == 0, 'native world client failed')
        for log in (one_log, two_log):
            require('confirmed=1 sockets=0' in log.read_text(), 'native Bye not confirmed')
        first, second = states(one_log), states(two_log)
        require(first[0]['selection'] == 0 and second[0]['selection'] == 0, 'initial world pose must be clip0')
        by_revision = {state['revision']: state for state in first}
        shared = [state for state in second if state['revision'] in by_revision]
        require(len(shared) >= 30, 'too few jointly observed snapshots')
        require(all(state == by_revision[state['revision']] for state in shared), 'native clients disagreed on same authoritative snapshot')
        for name, logs in [('one', first), ('two', second)]:
            trajectory = [state['members'][name] for state in logs if name in state['members']]
            require(any(state['blocked'] for state in trajectory), 'real wall did not block '+name)
            require(math.hypot(trajectory[-1]['x']-trajectory[0]['x'], trajectory[-1]['z']-trajectory[0]['z']) > 50, 'native player did not move')
            require(trajectory[-1]['applied'] > 10, 'movement acknowledgments missing')
            for before, after in zip(logs, logs[1:]):
                if name in before['members'] and name in after['members']:
                    a, b = before['members'][name], after['members'][name]
                    distance = math.hypot(b['x']-a['x'], b['z']-a['z'])
                    require(distance <= (after['tick']-before['tick'])*7.5+1e-6, 'client intent bypassed fixed server speed/tick')
        require(any('one' not in state['members'] and 'two' in state['members'] for state in second), 'peer departure absent from remaining client snapshots')
        checks.append(f'separate server + two native clients: {len(shared)} equal snapshots, real movement/wall blocking, bounded speed, membership removal and clean Bye')
        checks.append('four participants maximum')

        # Startup hash covers changed texture bytes and changed simulation parameters.
        texture = content/next(name for name in identity['files'] if name.endswith('.png'))
        old = texture.read_bytes()
        texture.write_bytes(old+b'\0')
        altered = identify(recipe)['digest']
        changed, changed_log = launch('changed-texture', ['client', *args, '--port', str(port), '--run-ms', '100'])
        require(changed.wait(timeout=4) == 2 and 'content_mismatch' in changed_log.read_text(), 'changed texture world joined server')
        require(altered != digest, 'texture did not contribute identity')
        texture.write_bytes(old)
        raw_recipe = recipe.read_text()
        recipe.write_text(raw_recipe.replace('speed\t150\n', 'speed\t151\n'))
        altered = identify(recipe)['digest']
        changed, changed_log = launch('changed-speed', ['client', *args, '--port', str(port), '--run-ms', '100'])
        require(changed.wait(timeout=4) == 2 and 'content_mismatch' in changed_log.read_text(), 'changed speed joined server')
        require(altered != digest, 'parameters did not contribute identity')
        recipe.write_text(raw_recipe)
        checks.append('texture bytes and simulation-parameter mismatches rejected before membership')

        # One replaceable intention: a burst does not create extra simulation steps.
        held = joined('held')
        held.sendall(b''.join(packet(9, struct.pack('!Qhhh', sequence, 1000, 0, 0)) for sequence in range(1, 21)))
        observed, last_ping = [], time.monotonic()
        deadline = time.monotonic()+0.85
        while time.monotonic() < deadline:
            if time.monotonic()-last_ping >= 0.15:
                held.sendall(packet(5, struct.pack('!Q', 1)))
                last_ping = time.monotonic()
            kind, body = receive(held)
            if kind == 3:
                state = decode_state(body)
                if 'held' in state['members']:
                    observed.append((state['tick'], state['members']['held']))
        applied = [p for _, p in observed if p['applied'] == 20]
        require(applied and any(p['expired'] for p in applied), 'held input did not expire')
        require(max(p['x'] for p in applied)-(-61.14208710193634) <= 37.5+1e-6, 'burst/expired intention advanced too far')
        require(len(applied) >= 5 and all(abs(p['x']-applied[-1]['x']) < 1e-9 for p in applied[-4:]), 'actor drifted after intent expiry')
        finish(held)
        checks.append('twenty coalesced inputs occupy one current intention; 250ms expiry stops drift without disconnect')

        malformed = [
            ('version', packet(1, version=1), b'protocol_version'),
            ('length', packet(1, length=0xffffffff), b'payload_limit'),
            ('digest', hello('bad', '00'*32), b'content_mismatch'),
            ('prehello', packet(9, struct.pack('!Qhhh', 1, 0, 0, 0)), b'hello_required'),
        ]
        for name, data, reason in malformed:
            peer = connect(port)
            peer.sendall(data)
            require(until(peer, 8)[1:] == reason, 'wrong rejection '+name)
            peer.close()
        for name, data, reason in [
            ('range', struct.pack('!Qhhh', 1, 1001, 0, 0), b'motion_range'),
            ('diagonal', struct.pack('!Qhhh', 1, 1000, 1000, 0), b'motion_range'),
            ('turn', struct.pack('!Qhhh', 1, 0, 0, -1001), b'motion_range'),
            ('sequence', struct.pack('!Qhhh', 2, 0, 0, 0), b'motion_sequence'),
            ('overflow', struct.pack('!Qhhh', 2**64-1, 0, 0, 0), b'motion_sequence'),
            ('position', struct.pack('!Qhhhddd', 1, 0, 0, 0, 1e6, 1e6, 1e6), b'trailing_payload'),
        ]:
            peer = joined(name)
            peer.sendall(packet(9, data))
            require(until(peer, 8)[1:] == reason, 'wrong motion rejection '+name)
            peer.close()
        rate = joined('rate')
        rate.sendall(b''.join(packet(9, struct.pack('!Qhhh', index, 0, 0, 0)) for index in range(1, 42)))
        require(until(rate, 8)[1:] == b'motion_rate_limit', 'motion-specific rate limit not enforced')
        rate.close()
        checks.append('version, digest, length, pre-handshake, axis/norm/yaw, sequence/overflow, client position payload and motion-rate violations rejected')
        peer = connect(port)
        require(until(peer, 8)[1:] == b'handshake_timeout', 'handshake deadline missing')
        peer.close()
        peer = joined('partial')
        peer.sendall(packet(9, b'\0', length=14))
        require(until(peer, 8)[1:] == b'partial_frame_timeout', 'partial motion deadline missing')
        peer.close()
        peer = joined('quiet')
        require(until(peer, 8)[1:] == b'idle_timeout', 'idle input deadline missing')
        peer.close()
        peer = connect(port)
        fragmented = hello('fragmented')
        for at in range(0, len(fragmented), 6):
            peer.sendall(fragmented[at:at+6])
            time.sleep(0.005)
        until(peer, 2)
        peer.sendall(packet(9, struct.pack('!Qhhh', 1, 0, 0, 0))+packet(5, struct.pack('!Q', 42)))
        require(until(peer, 6) == struct.pack('!Q', 42), 'coalesced motion and ping failed')
        finish(peer)
        checks.append('handshake/partial/idle deadlines, fragmented Hello and coalesced motion/heartbeat retained')
        if os.name != 'nt':
            for stop_signal in (signal.SIGINT, signal.SIGTERM):
                child, log = launch('signal-'+stop_signal.name, ['client', *args, '--port', str(port), '--name', stop_signal.name, '--run-ms', '15000'])
                participant = wait_text(log, r'WELCOME id=(\d+)').group(1)
                child.send_signal(stop_signal)
                require(child.wait(timeout=3) == 0 and 'confirmed=1 sockets=0' in log.read_text(), 'signal lost graceful shutdown')
                wait_text(server_log, rf'LEAVE id={participant} reason=client_bye')
            checks.append('world-client SIGINT/SIGTERM use bounded Bye and server-observed departure')
        survivor, survivor_log = launch('survivor', ['client', *args, '--port', str(port), '--name', 'survivor', '--run-ms', '20000'])
        wait_text(survivor_log, r'WORLD_STATE')
        require(server.wait(timeout=15) == 0, 'world server failed cleanup')
        require(survivor.wait(timeout=3) == 0 and 'confirmed=1 sockets=0' in survivor_log.read_text(), 'live world client failed server shutdown')
        require('STOP peers=0 sockets=0' in server_log.read_text(), 'server sockets not closed')
        try:
            probe = socket.create_connection(('127.0.0.1', port), timeout=0.2)
        except OSError:
            pass
        else:
            probe.close()
            raise AssertionError('listener remains after shutdown')
        checks.append('server usable after invalid traffic; live client confirms shutdown and listening socket is closed')
        report = {'status': 'passed', 'protocol': 2, 'transport': 'TCP loopback', 'world_digest': digest,
                  'world_files': len(identity['files']), 'shared_snapshots': len(shared), 'binary': str(binary),
                  'original_game_simulation': False, 'process_ids': [p.pid for p in children], 'checks': checks}
        (output/'validation.json').write_text(json.dumps(report, indent=2)+'\n')
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
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--world', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.output:
        args.output.mkdir(parents=True, exist_ok=False)
        run(args.binary.resolve(), args.world.resolve(), args.output.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix='mh4u-network-world-') as name:
            run(args.binary.resolve(), args.world.resolve(), Path(name))


if __name__ == '__main__':
    main()
