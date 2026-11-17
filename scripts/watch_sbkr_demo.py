#!/usr/bin/env python3
"""Show the demo gateway's saved values in a live terminal table."""
import argparse
import json
import os
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--store', type=Path, default=ROOT / '.run/sbkr-store.json')
    parser.add_argument('--once', action='store_true')
    args = parser.parse_args()
    descriptions = {p['sbkr_id']: p['description'] for p in
                    json.loads((ROOT / 'config/dtm.sbkr-demo.json').read_text())['io_points']}
    run = ROOT / '.run'
    run.mkdir(exist_ok=True)
    marker = run / 'terminal-store.pid'
    if not args.once:
        marker.write_text(str(os.getpid()) + '\n')
    previous = None
    try:
        while True:
            try:
                text = args.store.read_text()
                if text != previous:
                    saved = json.loads(text)
                    points = saved['points']
                    lines = ['SBKR DEMO: SAVED GATEWAY READINGS (synthetic meter values)',
                             f'Store: {args.store}',
                             f'Acknowledged batches: {saved["write_batches"]} | Points: {len(points)} | '
                             f'GOOD: {sum(p["quality"] == 2 for p in points)}',
                             '', f'{"SBKR ID":<10} {"Measurement":<40} {"Value":>18}  Quality', '-' * 85]
                    for point in points:
                        name = descriptions.get(point['id'], 'Additional demo point')
                        quality = 'GOOD' if point['quality'] == 2 else str(point['quality'])
                        lines.append(f'{point["id"]:<10} {name:<40} {point["value"]:>18.10g}  {quality}')
                    lines += ['', 'Gateway acknowledgment means the local demo store accepted the write.',
                              'Ctrl+C closes this monitor; the meter, client and gateway keep running.']
                    if sys.stdout.isatty():
                        print('\033[2J\033[H', end='')
                    print('\n'.join(lines), flush=True)
                    previous = text
            except (OSError, ValueError, KeyError) as error:
                if args.once:
                    print(f'Store unavailable: {error}', file=sys.stderr)
                    return 1
            if args.once:
                return 0
            time.sleep(1)
    except KeyboardInterrupt:
        return 0
    finally:
        if not args.once and marker.exists() and marker.read_text().strip() == str(os.getpid()):
            marker.unlink()

if __name__ == '__main__':
    sys.exit(main())
