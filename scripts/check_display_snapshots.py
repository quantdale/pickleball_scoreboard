"""Independent shared-asset pixel oracle + actual C++ export comparison.

--update regenerates versioned test fixtures after an approved layout change.
Optional --images writes PNG evidence ONLY to the supplied session directory.
"""
import argparse
import csv
import io
import json
import math
import struct
import subprocess
import zlib
from pathlib import Path
from typing import Any, TypedDict


class StateCase(TypedDict):
    name: str
    leftScore: int
    rightScore: int
    servingSide: str
    serverNumber: int
    gameEnded: bool
    gameMode: str


class Snapshot(StateCase):
    rows: list[str]


def load_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text())
    except (OSError, ValueError) as exc:
        raise SystemExit(f'Cannot load fixture/asset {path}: {exc}') from exc


ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / 'shared/render_fixtures'


def reference(state: StateCase, font, arrow, layout):
    pixels = set()
    scale = layout['digitScale']
    for score, center in [(state['leftScore'], layout['leftCenterX']),
                          (state['rightScore'], layout['rightCenterX'])]:
        text = str(score) if 0 <= score <= 99 else '--'
        width = len(text) * 5 * scale + (len(text) - 1) * layout['digitGap']
        start = math.floor(center - width / 2 + 0.5)
        for i, digit in enumerate(text):
            origin = start + i * (5 * scale + layout['digitGap'])
            if digit == '-':
                pixels.update((origin+x, y) for x in range(10) for y in (7, 8))
            else:
                for y, row in enumerate(font[digit]):
                    for x, bit in enumerate(row):
                        if bit == '1':
                            pixels.update((origin+x*scale+dx, layout['digitTopY']+y*scale+dy)
                                          for dx in range(scale) for dy in range(scale))
    count = state['serverNumber']
    left = state['servingSide'] == 'L'
    origin = layout['leftArrowX' if left else 'rightArrowX']
    top = layout['arrowSingleTopY' if count == 1 else 'arrowDoubleTopY']
    for i in range(count):
        for y, row in enumerate(arrow):
            for x, bit in enumerate(row[::-1] if left else row):
                if bit == '1':
                    pixels.add((origin+x, top+i*(len(arrow)+layout['arrowGap'])+y))
    assert all(0 < x < 63 and 0 < y < 15 and x not in range(30, 34) for x, y in pixels)
    return [''.join('1' if (x, y) in pixels else '0' for x in range(64)) for y in range(16)]


def cases() -> list[StateCase]:
    result: list[StateCase] = []
    for index, (left, right) in enumerate([(0, 0), (1, 0), (10, 25), (25, 10), (99, 99), (100, -1)]):
        for side in ['L', 'R']:
            for server in [1, 2]:
                for mode in ['D', 'S']:
                    result.append({'name': f'{left}-{right}-{side}{server}-{mode}',
                                   'leftScore': left, 'rightScore': right, 'servingSide': side,
                                   'serverNumber': server, 'gameEnded': index == 3, 'gameMode': mode})
    return result


def png(path, rows, scale=8):
    width, height = len(rows[0])*scale, len(rows)*scale
    raw = bytearray()
    for row in rows:
        data = b''.join((b'\xff\xff\xff' if bit == '1' else b'\x00\x00\x00') * scale for bit in row)
        raw.extend((b'\x00'+data)*scale)
    def chunk(kind, value):
        return struct.pack('!I', len(value)) + kind + value + struct.pack('!I', zlib.crc32(kind+value))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('!2I5B', width, height, 8, 2, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(bytes(raw))) + chunk(b'IEND', b''))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--update', action='store_true')
    parser.add_argument('--cpp-tool', type=Path)
    parser.add_argument('--images', type=Path)
    args = parser.parse_args()
    assets = ROOT / 'shared/display_assets'
    font = load_json(assets/'font_5x7.json')
    arrow = load_json(assets/'arrows.json')['ARROW_RIGHT']
    layout = load_json(assets/'layout.json')
    expected: list[Snapshot] = []
    for state in cases():
        snapshot: Snapshot = {**state, 'rows': reference(state, font, arrow, layout)}
        expected.append(snapshot)
    table = io.StringIO()
    writer = csv.writer(table, lineterminator='\n')
    writer.writerow(['name', 'left', 'right', 'side', 'server', 'ended', 'mode'])
    for case in expected:
        writer.writerow([case['name'], case['leftScore'], case['rightScore'], case['servingSide'],
                         case['serverNumber'], int(case['gameEnded']), case['gameMode']])
    if args.update:
        FIXTURES.mkdir(parents=True, exist_ok=True)
        (FIXTURES/'scoreboard_snapshots.json').write_text(json.dumps(expected, indent=2)+'\n')
        (FIXTURES/'cases.csv').write_text(table.getvalue())
    actual_fixture = load_json(FIXTURES/'scoreboard_snapshots.json')
    assert expected == actual_fixture, 'Shared-asset oracle differs from committed pixel fixtures'
    if args.cpp_tool:
        output = subprocess.check_output([str(args.cpp_tool.resolve())], input=table.getvalue(), text=True).splitlines()
        assert len(output) == len(expected)*17
        for index, case in enumerate(expected):
            block = output[index*17:(index+1)*17]
            assert block[0] == case['name'] and block[1:] == case['rows'], case['name']
    if args.images:
        if args.images.resolve().is_relative_to(ROOT):
            parser.error('Image evidence must be outside the repository')
        args.images.mkdir(parents=True, exist_ok=True)
        for case in expected:
            png(args.images/(case['name']+'.png'), case['rows'])
    print(f'PASS: {len(expected)} complete 64x16 pixel snapshots; C++ compared={bool(args.cpp_tool)}')


if __name__ == '__main__':
    main()
