#!/usr/bin/env python3
"""Run one demo component in its own visible terminal, then leave a shell open."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
COMMANDS = {
    'meter': ['./build/dlms_meter_simulator', 'config/dtm.sbkr-demo.json'],
    'gateway': ['./build/sbkr_demo_gateway', 'tcp://127.0.0.1:15555', '.run/sbkr-store.json'],
    'client': ['./build/dlms_client_sbkr', 'config/dtm.sbkr-demo.json'],
    'store': ['python3', 'scripts/watch_sbkr_demo.py'],
}

def main():
    if len(sys.argv) != 2 or sys.argv[1] not in COMMANDS:
        print('Usage: python3 scripts/demo_terminal.py meter|gateway|client|store')
        return 2
    component = sys.argv[1]
    os.chdir(ROOT)
    run = ROOT / '.run'
    run.mkdir(exist_ok=True)
    print(f'\033]0;DLMS SBKR DEMO - {component.upper()}\007', end='', flush=True)
    print(f'DLMS SBKR DEMO - {component.upper()}\nProject: {ROOT}\n'
          f'Command: {shlex.join(COMMANDS[component])}\n'
          'These are local demo services with synthetic meter values.\n'
          'Press Ctrl+C to stop this component.\n', flush=True)
    process = subprocess.Popen(COMMANDS[component])
    marker = run / f'terminal-{component}.json'
    marker.write_text(json.dumps({'terminal_pid': os.getpid(), 'component_pid': process.pid,
                                  'command': COMMANDS[component]}, indent=2) + '\n')
    try:
        code = process.wait()
    except KeyboardInterrupt:
        process.terminate()
        try:
            process.wait(timeout=4)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        code = process.returncode
    finally:
        if marker.exists():
            marker.unlink()
    print(f'\n{component} stopped (exit {code}). You can run commands below.\n', flush=True)
    os.execv('/bin/bash', ['bash', '-i'])

if __name__ == '__main__':
    sys.exit(main())
