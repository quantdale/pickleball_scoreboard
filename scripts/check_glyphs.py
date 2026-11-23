#!/usr/bin/env python3
"""Spec 03 Section 6: compare shared JSON assets with firmware static copies."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'shared/display_assets'
LAYOUT_CONSTANTS = {
    'canvasWidth': 'CANVAS_WIDTH', 'canvasHeight': 'CANVAS_HEIGHT',
    'digitTopY': 'DIGIT_TOP_Y', 'digitScaleX': 'DIGIT_SCALE_X', 'digitScaleY': 'DIGIT_SCALE_Y',
    'digitGap': 'DIGIT_GAP', 'leftScoreX': 'LEFT_SCORE_X', 'rightScoreX': 'RIGHT_SCORE_X',
    'scoreWidth': 'SCORE_WIDTH', 'arrowTopY': 'ARROW_TOP_Y', 'arrowGap': 'ARROW_GAP',
}


def load(path):
    try:
        return json.loads(path.read_text(encoding='utf-8'))
    except (OSError, ValueError) as exc:
        raise SystemExit(f'Cannot read {path}: {exc}') from exc


def literals(cpp, declaration):
    match = re.search(re.escape(declaration) + r'\s*=\s*\{(.*?)\n\};', cpp, re.S)
    if not match:
        raise SystemExit('Missing C++ declaration: ' + declaration)
    return re.findall(r'0b([01]+)', match.group(1))


def main():
    cpp = (ROOT / 'firmware/src/display_render_logic.cpp').read_text(encoding='utf-8')
    font = load(ASSETS / 'font_5x7.json')
    arrows = load(ASSETS / 'arrows.json')['ARROW_RIGHT']
    layout = load(ASSETS / 'layout.json')
    expected_font = [row for digit in range(10) for row in font[str(digit)]]
    if expected_font != literals(cpp, 'FONT_5X7[10][7]'):
        raise SystemExit('Digit glyph drift')
    if len(arrows) != 12 or any(len(row) != 6 for row in arrows):
        raise SystemExit('Arrow dimensions must be 6 wide by 12 tall')
    if arrows != literals(cpp, 'ARROW_RIGHT[12]'):
        raise SystemExit('Arrow glyph drift')
    values = dict(re.findall(r'constexpr\s+int\s+(\w+)\s*=\s*([0-9]+);', cpp))
    if set(layout) != set(LAYOUT_CONSTANTS):
        raise SystemExit('Layout keys differ from the checked constant list')
    for key, constant in LAYOUT_CONSTANTS.items():
        if constant not in values or int(values[constant]) != layout[key]:
            raise SystemExit(f'Layout drift: {key}/{constant}')
    print(f'PASS: 10 digit glyphs, 6x12 serving arrow and {len(LAYOUT_CONSTANTS)} layout constants in sync')


if __name__ == '__main__':
    main()
