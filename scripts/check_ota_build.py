"""Inspect/package already-built OTA images without opening serial or flashing.

Copies only public firmware/recovery assets, never signing keys or credentials.
Requires an explicit preserved-baseline directory, not a possibly rebuilt app.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
from pathlib import Path

import ota_tool


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def partitions(path):
    result = []
    data = path.read_bytes()
    for offset in range(0, len(data), 32):
        magic, typ, subtype, address, size, name, _ = struct.unpack_from('<HBBII16sI', data, offset)
        if magic != 0x50AA:
            break
        result.append({'name': name.rstrip(b'\0').decode(), 'type': typ, 'subtype': subtype,
                       'offset': address, 'bytes': size})
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--baseline-dir', type=Path, required=True)
    parser.add_argument('--platformio-home', type=Path, default=Path.home()/'.platformio')
    args = parser.parse_args()
    output = ota_tool.outside(args.output_dir)
    if output.exists():
        parser.error('Use a new output directory; preserve previous manifests/images')
    args.baseline_dir = args.baseline_dir.resolve()
    expected = '14e77d5d48aee05652276a2757277439e89dff6d92614b76a0882294638228ae'
    if digest(args.baseline_dir/'firmware.bin') != expected:
        parser.error('Baseline is not the actually commissioned 3bf53dd application')
    packages = args.platformio_home/'packages'
    framework = packages/'framework-arduinoespressif32'
    executable_suffix = '.exe' if os.name == 'nt' else ''
    tools = packages/'toolchain-xtensa-esp32/bin'
    config = (framework/'tools/sdk/esp32/dio_qspi/include/sdkconfig.h').read_text()
    if '#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1' not in config:
        parser.error('Inspected SDK no longer enables application rollback')
    if '#define CONFIG_SECURE_BOOT ' in config or '#define CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT ' in config:
        parser.error('Security configuration changed; re-audit signing/boot assumptions')
    output.mkdir(parents=True)
    bootloader = framework/'tools/sdk/esp32/bin/bootloader_dio_40m.elf'
    disassembly = subprocess.check_output([str(tools/('xtensa-esp32-elf-objdump'+executable_suffix)),
        '-d', '--disassemble=bootloader_utility_get_selected_boot_partition', str(bootloader)], text=True)
    # Actual ELF evidence: reads ota_state offset24, compares1 (PENDING), writes4
    # (ABORTED), then later compares0 (NEW) and writes1 (PENDING) via write_otadata.
    for pattern in ['write_otadata', 'bootloader_common_get_active_otadata', 'movi.n\ta8, 4', 'movi.n\ta8, 1']:
        if pattern not in disassembly:
            parser.error('Bootloader ELF rollback state machine changed; inspect it manually')
    (output/'bootloader-rollback-disassembly.log').write_text(disassembly)
    report = {'hardwareTested': False, 'serialPortOpened': False, 'flashWritten': False,
              'baselineApplicationSha256': expected, 'sdkRollbackEnabled': True,
              'bootloaderElfSha256': digest(bootloader), 'rollbackInstructionsPresentInInspectedElf': True,
              'hardwareRollbackDemonstrated': False, 'requiredFlashBytes': 0x400000,
              'slotBytes': ota_tool.SLOT_SIZE, 'variants': [], 'files': []}
    for variant, build in [('ota', 'ota-v1-a'), ('ota_b', 'ota-v1-b')]:
        directory = ota_tool.ROOT/'firmware/.pio/build'/variant
        image = (directory/'firmware.bin').read_bytes()
        ota_tool.validate_image(image, build)
        table = partitions(directory/'partitions.bin')
        apps = [item for item in table if item['type'] == 0]
        if [(app['offset'], app['bytes']) for app in apps] != [(0x10000, ota_tool.SLOT_SIZE), (0x200000, ota_tool.SLOT_SIZE)]:
            parser.error('Actual built OTA table does not match required layout')
        if table[-1]['offset']+table[-1]['bytes'] != 0x400000:
            parser.error('Partition table does not exactly fit4MB')
        if digest(directory/'bootloader.bin') != digest(args.baseline_dir/'bootloader.bin'):
            parser.error('Bootloader differs from installed baseline; separate migration audit required')
        nm = subprocess.check_output([str(tools/('xtensa-esp32-elf-nm'+executable_suffix)), str(directory/'firmware.elf')], text=True)
        if not re.search(r' T verifyRollbackLater$', nm, re.M):
            parser.error('Deferred boot-confirmation override is not a strong linked symbol')
        size = subprocess.check_output([str(tools/('xtensa-esp32-elf-size'+executable_suffix)), '-A', str(directory/'firmware.elf')], text=True)
        (output/(variant+'-sections.log')).write_text(size)
        sections = {name: int(count) for name, count in re.findall(r'^(\.[\w.]+)\s+(\d+)\s+\d+', size, re.M)}
        report['variants'].append({'environment': variant, 'build': build, 'imageBytes': len(image),
            'slotHeadroomBytes': ota_tool.SLOT_SIZE-len(image),
            'staticDataAndBssBytes': sections['.dram0.data']+sections['.dram0.bss'],
            'instructionRamBytes': sections['.iram0.vectors']+sections['.iram0.text'],
            'heapRuntimeNotMeasured': True, 'partitionTable': table, 'deferredConfirmationLinked': True})
        target = output/variant
        target.mkdir()
        for name in ['firmware.bin', 'firmware.elf', 'partitions.bin', 'bootloader.bin']:
            shutil.copy2(directory/name, target/name)
    shutil.copytree(args.baseline_dir, output/'baseline-usb-recovery')
    shutil.copy2(framework/'tools/partitions/boot_app0.bin', output/'initial-otadata.bin')
    for path in sorted(output.rglob('*')):
        if path.is_file():
            report['files'].append({'path': path.relative_to(output).as_posix(), 'bytes': path.stat().st_size, 'sha256': digest(path)})
    (output/'build-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({key: value for key, value in report.items() if key != 'files'}, indent=2))
    print('PASS: public artifacts prepared; NO hardware access, signing key generation, or flash operation')


if __name__ == '__main__':
    main()
