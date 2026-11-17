#!/usr/bin/env python3
"""Run the simulated meter, temporary SBKR gateway, and publishing client."""
import argparse
import fcntl
import json
import logging
from logging.handlers import RotatingFileHandler
import os
from pathlib import Path
import selectors
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def atomic_json(path, value):
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--meter-port', type=int, default=14061)
    parser.add_argument('--gateway-port', type=int, default=15555)
    parser.add_argument('--output-dir', type=Path, default=ROOT / '.run')
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build')
    args = parser.parse_args()
    if not all(1 <= p <= 65535 for p in (args.meter_port, args.gateway_port)):
        parser.error('Ports must be between 1 and 65535')
    if args.meter_port == args.gateway_port:
        parser.error('Meter and gateway need separate ports')
    run = args.output_dir.resolve()
    run.mkdir(parents=True, exist_ok=True)
    lock = (run / 'demo.lock').open('a')
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        print('A demo already owns this output directory.', file=sys.stderr)
        return 2
    build = args.build_dir.resolve()
    for name in ('dlms_meter_simulator', 'sbkr_demo_gateway', 'dlms_client_sbkr', 'libsbkr_demo.so'):
        if not (build / name).is_file():
            print('Build first: cmake -S . -B build -DDLMS_SBKR_BUILD_DEMO=ON && cmake --build build -j4', file=sys.stderr)
            return 2
    endpoint = f'tcp://127.0.0.1:{args.gateway_port}'
    config = json.loads((ROOT / 'config/dtm.tcp-demo.json').read_text())
    config['connectivity']['polling_frequency'] = '1s'
    config['dlms'].update(port=args.meter_port, timeout_ms=1000)
    config['sbkr'] = dict(library=str(build / 'libsbkr_demo.so'), gateway=endpoint, logger_endpoint=endpoint)
    config_path = run / 'dtm.sbkr-demo.json'
    atomic_json(config_path, config)
    processes = {}
    logs = {}
    state = {'demo': True, 'running': False, 'supervisor_pid': os.getpid(),
             'meter': f'127.0.0.1:{args.meter_port}', 'gateway': endpoint,
             'store': str(run / 'sbkr-store.json'), 'pids': {}}
    stopped = False

    def stop(signum, frame):
        nonlocal stopped
        stopped = True

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)

    def launch(name, command, wait_ready=False):
        logger = logging.getLogger('sbkr-demo.' + name)
        logger.setLevel(logging.INFO)
        logger.propagate = False
        handler = RotatingFileHandler(run / (name + '.log'), maxBytes=1024 * 1024, backupCount=2)
        logger.addHandler(handler)
        logs[name] = logger
        proc = subprocess.Popen([str(x) for x in command], stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, cwd=ROOT, bufsize=0)
        processes[name] = proc
        state['pids'][name] = proc.pid
        if wait_ready:
            with selectors.DefaultSelector() as selector:
                selector.register(proc.stdout, selectors.EVENT_READ)
                selector.register(proc.stderr, selectors.EVENT_READ)
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline and not stopped:
                    for key, _ in selector.select(.2):
                        line = key.fileobj.readline().decode(errors='replace').rstrip()
                        if line:
                            logger.info(line)
                            if key.fileobj is proc.stdout:
                                print(line, flush=True)
                                return
                        if proc.poll() is not None:
                            raise RuntimeError(f'{name} failed to start; see {run / (name + ".log")}')
                raise RuntimeError(f'{name} readiness timed out')
        return proc

    result = 0
    try:
        launch('meter', [build / 'dlms_meter_simulator', config_path], True)
        launch('gateway', [build / 'sbkr_demo_gateway', endpoint, run / 'sbkr-store.json'], True)
        launch('client', [build / 'dlms_client_sbkr', config_path])
        state['running'] = True
        atomic_json(run / 'demo-status.json', state)
        print(f'DEMO running. Snapshot: {run / "sbkr-store.json"}. Ctrl+C stops all three processes.', flush=True)
        with selectors.DefaultSelector() as selector:
            for name, proc in processes.items():
                selector.register(proc.stdout, selectors.EVENT_READ, (name, 'stdout'))
                selector.register(proc.stderr, selectors.EVENT_READ, (name, 'stderr'))
            first = True
            while not stopped:
                for key, _ in selector.select(.5):
                    name, stream = key.data
                    raw = key.fileobj.readline()
                    if not raw:
                        selector.unregister(key.fileobj)
                        continue
                    line = raw.decode(errors='replace').rstrip()
                    if not line:
                        continue
                    if name == 'client' and stream == 'stdout' and line.startswith('{'):
                        snapshot = json.loads(line)
                        snapshot.update(demo=True, source='dlms_meter_simulator')
                        atomic_json(run / 'latest.json', snapshot)
                        state['last_observation_ms'] = max(p['time_ms'] for p in snapshot['points'])
                        state['good_points'] = sum(p['quality'] == 2 for p in snapshot['points'])
                        atomic_json(run / 'demo-status.json', state)
                        if first:
                            print(f'{state["good_points"]}/{len(snapshot["points"])} meter readings GOOD.', flush=True)
                            first = False
                    else:
                        logs[name].info(line)
                        if name == 'client':
                            state['last_client_message'] = line
                            atomic_json(run / 'demo-status.json', state)
                for name, proc in processes.items():
                    if proc.poll() is not None:
                        raise RuntimeError(f'{name} exited ({proc.returncode}); see {run / (name + ".log")}')
    except (OSError, ValueError, RuntimeError) as error:
        print(error, file=sys.stderr)
        result = 1
    finally:
        for proc in reversed(list(processes.values())):
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=4)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()
            proc.stdout.close()
            proc.stderr.close()
        state['running'] = False
        atomic_json(run / 'demo-status.json', state)
        lock.close()
    return result


if __name__ == '__main__':
    sys.exit(main())
