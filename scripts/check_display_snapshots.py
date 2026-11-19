"""Independent shared-asset pixel oracle + actual C++ export comparison.

--update regenerates versioned test fixtures after an approved layout change.
Optional --images writes PNG evidence ONLY to the supplied session directory.
"""
import argparse
import csv
import io
import json
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
    # Independent inverse sampler: for every canvas cell, find its source bit.
    # Assert the physical canvas and band positions independently of the layout asset.
    width, height = layout['canvasWidth'], layout['canvasHeight']
    assert (width, height) == (16, 64)
    assert (layout['servingTopY'], layout['opponentTopY'], layout['serverTopY'], layout['arrowTopY']) == (1, 17, 36, 53)
    left = state['servingSide'] == 'L'
    scores = [state['leftScore'], state['rightScore']]
    if not left:
        scores.reverse()
    bands = list(zip((1, 17, 36), scores + [state['serverNumber']], strict=True))

    def sample(x, y):
        for top, value in bands:
            if top <= y < top + 14:
                text = str(value) if 0 <= value <= 99 else '--'
                stretch = 2 if len(text) == 1 else 1
                local_x = x - (3 if len(text) == 1 else 2)
                glyph, col = divmod(local_x, 5 * stretch + 2)
                if not 0 <= glyph < len(text) or col >= 5 * stretch:
                    return '0'
                row = (y - top) // 2
                return ('1' if row == 3 else '0') if text[glyph] == '-' else font[text[glyph]][row][col // stretch]
        if 53 <= y <= 62 and 3 <= x <= 12:
            col = (x - 3) // 2
            return arrow[(y - 53) // 2][4 - col if left else col]
        return '0'

    return [''.join(sample(x, y) for x in range(width)) for y in range(height)]


def cases() -> list[StateCase]:
    result: list[StateCase] = []
    for left, right in [(0, 0), (1, 8), (2, 3), (3, 2), (10, 25), (25, 10), (88, 99), (99, 99), (100, -1)]:
        for side in ['L', 'R']:
            for server in [1, 2]:
                for mode in ['D', 'S']:
                    result.append({'name': f'{left}-{right}-{side}{server}-{mode}',
                                   'leftScore': left, 'rightScore': right, 'servingSide': side,
                                   'serverNumber': server, 'gameEnded': (left, right) == (25, 10), 'gameMode': mode})
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
        assert len(output) == len(expected)*65
        for index, case in enumerate(expected):
            block = output[index*65:(index+1)*65]
            assert block[0] == case['name'] and block[1:] == case['rows'], case['name']
    if args.images:
        if args.images.resolve().is_relative_to(ROOT):
            parser.error('Image evidence must be outside the repository')
        args.images.mkdir(parents=True, exist_ok=True)
        for case in expected:
            png(args.images/(case['name']+'.png'), case['rows'])
        # Offline comparison cards; no diagnostic mode or scoring injection on the board.
        rows = next(case['rows'] for case in expected if case['name'] == '2-3-L2-D')
        top, bottom = rows[:32], rows[32:]
        alternatives = {
            'panel-swap-upright': bottom + top,
            'whole-r180': [row[::-1] for row in reversed(rows)],
            'horizontal-mirror': [row[::-1] for row in rows],
            'top-panel-r180': [row[::-1] for row in reversed(top)] + bottom,
            'bottom-panel-r180': top + [row[::-1] for row in reversed(bottom)],
        }
        for fault, visible in alternatives.items():
            png(args.images / f'orientation-2-3-L2-{fault}.png', visible)
    print(f'PASS: {len(expected)} complete 16x64 pixel snapshots; C++ compared={bool(args.cpp_tool)}')


if __name__ == '__main__':
    main()
