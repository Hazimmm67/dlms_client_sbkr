"""The launcher must continue monitoring after empty transport diagnostic lines."""
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time

script, build_dir = sys.argv[1:]

def port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]

with tempfile.TemporaryDirectory() as directory:
    output = Path(directory)
    with (output / 'runner.log').open('wb') as log:
        runner = subprocess.Popen([sys.executable, '-u', script, '--build-dir', build_dir,
                                   '--meter-port', str(port()), '--gateway-port', str(port()),
                                   '--output-dir', directory], stdout=log, stderr=subprocess.STDOUT)
        meter_pid = None
        frozen = False

        def wait_for(predicate, timeout=15):
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                assert runner.poll() is None, (output / 'runner.log').read_text()
                try:
                    status = json.loads((output / 'demo-status.json').read_text())
                    latest = json.loads((output / 'latest.json').read_text())
                    if predicate(status, latest):
                        return status, latest
                except (FileNotFoundError, json.JSONDecodeError):
                    pass
                time.sleep(.05)
            raise AssertionError('Runner state did not update: ' + (output / 'runner.log').read_text())

        try:
            status, first = wait_for(lambda s, p: s.get('good_points') == 25)
            meter_pid = status['pids']['meter']
            os.kill(meter_pid, signal.SIGSTOP)
            frozen = True
            status, failed = wait_for(lambda s, p: s.get('good_points') == 0 and
                                     all(point['quality'] != 2 for point in p['points']))
            assert failed['points'][0]['time_ms'] > first['points'][0]['time_ms']
            os.kill(meter_pid, signal.SIGCONT)
            frozen = False
            status, recovered = wait_for(lambda s, p: s.get('good_points') == 25 and
                                        p['points'][0]['time_ms'] > failed['points'][0]['time_ms'])
            assert len(recovered['points']) == 25
            assert all(point['quality'] == 2 for point in recovered['points'])
        finally:
            if frozen and meter_pid:
                os.kill(meter_pid, signal.SIGCONT)
            runner.terminate()
            try:
                runner.wait(timeout=8)
            except subprocess.TimeoutExpired:
                runner.kill()
                runner.wait()
                raise AssertionError('Runner failed to stop its children')
        assert runner.returncode == 0, (output / 'runner.log').read_text()
        final_status = json.loads((output / 'demo-status.json').read_text())
        assert final_status['running'] is False
        for pid in final_status['pids'].values():
            assert not Path(f'/proc/{pid}').exists(), f'Demo child {pid} left running'
print('Launcher monitor survived meter timeout and recovery, and cleaned up all children')
