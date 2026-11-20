"""Independent shared-asset pixel oracle + actual C++ export comparison (Spec 03 §6).

--update regenerates the committed 64x16 fixtures from the oracle.
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
        return json.loads(path.read_text(encoding='utf-8'))
    except (OSError, ValueError) as exc:
        raise SystemExit(f'Cannot load fixture/asset {path}: {exc}') from exc


ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / 'shared/render_fixtures'


def reference(state: StateCase, font, arrow, layout):
    # Independent inverse sampler: for every canvas cell, find its source bit.
    # Spec 03 §4 coordinates are restated here, not derived from the layout asset.
    assert (layout['canvasWidth'], layout['canvasHeight']) == (64, 16)
    assert (layout['leftScoreX'], layout['rightScoreX'], layout['scoreWidth']) == (1, 41, 22)
    assert (layout['digitTopY'], layout['arrowTopY'], layout['digitGap'], layout['arrowGap']) == (1, 2, 2, 2)
    assert len(arrow) == 12 and all(len(row) == 6 for row in arrow)
    left = state['servingSide'] == 'L'
    pair = state['gameMode'] == 'D' and state['serverNumber'] == 2
    arrow_starts = (25, 33) if pair else (29,)

    def score_bit(x, y, start, value):
        text = str(value) if 0 <= value <= 99 else '--'
        local = x - start - (6 if len(text) == 1 else 0)
        glyph, col = divmod(local, 12)
        if not 0 <= glyph < len(text) or not 0 <= col < 10:
            return '0'
        row = (y - 1) // 2
        if text[glyph] == '-':
            return '1' if row == 3 else '0'
        return font[text[glyph]][row][col // 2]

    def sample(x, y):
        if 1 <= y <= 14 and 1 <= x <= 22:
            return score_bit(x, y, 1, state['leftScore'])
        if 1 <= y <= 14 and 41 <= x <= 62:
            return score_bit(x, y, 41, state['rightScore'])
        for start in arrow_starts:
            if start <= x < start + 6 and 2 <= y <= 13:
                col = x - start
                return arrow[y - 2][5 - col if left else col]
        return '0'

    return [''.join(sample(x, y) for x in range(64)) for y in range(16)]


def cases() -> list[StateCase]:
    result: list[StateCase] = []
    pairs = [(0, 0), (1, 8), (7, 5), (5, 2), (3, 3), (10, 25), (25, 10), (88, 99), (99, 99), (100, -1)]
    for left, right in pairs:
        for side in ['L', 'R']:
            for mode, server in [('D', 1), ('D', 2), ('S', 1)]:
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
    expected: list[Snapshot] = [{**state, 'rows': reference(state, font, arrow, layout)} for state in cases()]
    table = io.StringIO()
    writer = csv.writer(table, lineterminator='\n')
    writer.writerow(['name', 'left', 'right', 'side', 'server', 'ended', 'mode'])
    for case in expected:
        writer.writerow([case['name'], case['leftScore'], case['rightScore'], case['servingSide'],
                         case['serverNumber'], int(case['gameEnded']), case['gameMode']])
    if args.update:
        (FIXTURES/'scoreboard_snapshots.json').write_text(json.dumps(expected, indent=2)+'\n', encoding='utf-8')
        (FIXTURES/'cases.csv').write_text(table.getvalue(), encoding='utf-8')
    assert expected == load_json(FIXTURES/'scoreboard_snapshots.json'), 'Oracle differs from committed fixtures'
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
        # What a viewer would see from the default image on the other build mounts (Spec 03 §5).
        rows = next(case['rows'] for case in expected if case['name'] == '5-2-L2-D')
        alternatives = {
            'mount-2-counterclockwise': [row[::-1] for row in reversed(rows)],
            'mount-3-bringup-order': [row[32:] + row[:32] for row in rows],
        }
        for fault, visible in alternatives.items():
            png(args.images / f'wrong-{fault}-5-2-L2.png', visible)
    print(f'PASS: {len(expected)} complete 64x16 pixel snapshots; C++ compared={bool(args.cpp_tool)}')


if __name__ == '__main__':
    main()
