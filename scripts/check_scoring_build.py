"""Audit the linked ESP32 scoring memory and ATT ingress, without device access."""

import argparse
import os
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROFILES = ('esp32dev', 'ota', 'ota_b')
JOURNAL_BYTES = 96  # Spec 01 §6: 32 three-byte positions
# Static DRAM is OTA heap headroom. 2.2 measured 62,436 B after shrinking the
# journal from 6,144 B; reassess the measured OTA margin before raising this cap.
OTA_STATIC_LIMIT = 63000


def check_profile(tools, profile):
    elf = ROOT / 'firmware/.pio/build' / profile / 'firmware.elf'

    def run(tool, *args):
        suffix = '.exe' if os.name == 'nt' else ''
        program = tools / f'xtensa-esp32-elf-{tool}{suffix}'
        return subprocess.check_output([str(program), *args, str(elf)], text=True)

    def require(condition, message):
        if not condition:
            raise SystemExit(f'{profile}: {message}')

    names = run('nm', '-S', '--defined-only')
    sizes = run('size', '-A')
    sections = {name: int(size) for name, size in re.findall(r'^(\.\S+)\s+(\d+)\s+\d+', sizes, re.M)}
    dram = sections['.dram0.data'] + sections['.dram0.bss']
    require(profile not in ('ota', 'ota_b') or dram <= OTA_STATIC_LIMIT,
            f'static DRAM grew to {dram} B (limit {OTA_STATIC_LIMIT} B); review OTA memory evidence')
    state = re.search(r'^\w+ (\w+) b .*currentState\w*$', names, re.M)
    state_bytes = int(state[1], 16) if state else 0
    require(JOURNAL_BYTES <= state_bytes <= JOURNAL_BYTES + 32,
            f'expected one static scoring state with a {JOURNAL_BYTES} B journal')
    require('admitHistoryBlock' not in names, 'scoring must not depend on a heap reserve')

    def code(prefix):
        matches = [line.split()[-1] for line in names.splitlines()
                   if line.split()[-1].startswith(prefix)]
        require(len(matches) == 1, f'expected one linked function for {prefix}')
        return run('objdump', '-d', '--disassemble=' + matches[0])

    for function in ('_ZN9GameState6recordE', '_ZN9GameState4undoE'):
        calls = re.findall(r'\bcallx?\d+\s+([^\n]+)', code(function))
        require(all(call.endswith('<memcpy>') for call in calls),
                f'{function} unexpectedly calls {calls}')
    require('<__wrap_esp_ble_gatts_register_callback>' in code('_ZN9BLEDevice4initE'),
            'Arduino does not register through scoring ingress')
    require('<esp_ble_gatts_register_callback>' in code('__wrap_esp_ble_gatts_register_callback'),
            'scoring ingress does not register with Bluedroid')
    require('<xQueueGenericSend>' in code('_ZN12_GLOBAL__N_113dispatchGattsE'),
            'scoring ingress no longer queues command writes')
    print(f'{profile}: static DRAM={dram} B, GameState={state_bytes} B, '
          'allocation-free Undo and linked ATT ingress verified')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    home = Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home() / '.platformio'))
    parser.add_argument('--platformio-home', type=Path, default=home)
    parser.add_argument('profiles', nargs='*', default=PROFILES)
    args = parser.parse_args()
    tools = args.platformio_home / 'packages/toolchain-xtensa-esp32/bin'
    for profile in args.profiles:
        check_profile(tools, profile)


if __name__ == '__main__':
    main()
