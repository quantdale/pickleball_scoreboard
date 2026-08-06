#!/usr/bin/env python3
"""Drift checker: shared/display_assets JSON glyphs + layout vs the firmware's embedded C++ copies.

The design-time source of truth for glyphs is the JSON under
shared/display_assets/:
  font_5x7.json  - digits "0".."9", each a list of 7 strings of 5 '0'/'1' chars
  arrows.json    - ARROW_RIGHT, a list of 7 strings of 9 '0'/'1' chars
('1' = lit pixel; the first char of each row string is the left-most pixel.)

The Android app parses those JSON files at runtime, but the firmware embeds
hand-transcribed copies in firmware/src/display_render_logic.cpp as
`FONT_5X7[10][7]` (uint8_t) and `ARROW_RIGHT[7]` (uint16_t) row bitmasks -- an
approved deviation from Spec 03 Section 6 that must be kept in sync manually.
This script verifies the two stay in sync.

Layout constants are shared the same way: shared/display_assets/layout.json is
the design-time source of truth for the canvas size, score centers, arrow/digit
tops, gaps, and divider x (Spec 03 Sections 2/4); display_render_logic.cpp
hard-codes the same values as `constexpr` scalars, which this script checks too.

Bit order (confirmed against addDigit()/addArrow() in display_render_logic.cpp,
which test pixels with `(rowBits >> (WIDTH - 1 - col)) & 1`): the left-most
pixel is bit (WIDTH-1), so a row's binary literal text equals its JSON row text.

Usage: python scripts/check_glyphs.py
Exit status: 0 if the C++ copies match the JSON, 1 otherwise.
"""

import json
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
FONT_JSON_PATH = REPO_ROOT / "shared" / "display_assets" / "font_5x7.json"
ARROWS_JSON_PATH = REPO_ROOT / "shared" / "display_assets" / "arrows.json"
LAYOUT_JSON_PATH = REPO_ROOT / "shared" / "display_assets" / "layout.json"
RENDER_CPP_PATH = REPO_ROOT / "firmware" / "src" / "display_render_logic.cpp"

FONT_DIGITS = [str(d) for d in range(10)]
FONT_ROWS = 7
ARROW_ROWS = 7

# layout.json keys mapped to their `constexpr` declarations in the C++ source.
LAYOUT_CONSTANTS = {
    "canvasWidth": "CANVAS_WIDTH",
    "canvasHeight": "CANVAS_HEIGHT",
    "leftCenterX": "LEFT_CENTER_X",
    "rightCenterX": "RIGHT_CENTER_X",
    "arrowTopY": "ARROW_TOP_Y",
    "digitTopY": "DIGIT_TOP_Y",
    "digitGap": "DIGIT_GAP",
    "arrowGap": "ARROW_GAP",
    "dividerX": "dividerX",
}


def fail(message):
    print(f"ERROR: {message}")
    sys.exit(1)


def load_json(path):
    try:
        with open(path, encoding="utf-8") as handle:
            return json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"cannot read/parse {path}: {exc}")


def extract_literals(cpp_text, declaration, expected_count):
    """Return the `0b...` row literals inside `declaration = { ... };` in the
    C++ source, as strings of '0'/'1' digits in source order."""
    pattern = re.compile(re.escape(declaration) + r"\s*=\s*\{(.*?)\n\};", re.DOTALL)
    match = pattern.search(cpp_text)
    if match is None:
        fail(f"could not find `{declaration} = {{ ... }};` in {RENDER_CPP_PATH}")
    literals = re.findall(r"0b([01]+)", match.group(1))
    if len(literals) != expected_count:
        fail(
            f"`{declaration}` in {RENDER_CPP_PATH} has {len(literals)} row "
            f"literals, expected {expected_count}"
        )
    return literals


def decode_row(value, width):
    """Decode a row bitmask to a '0'/'1' string, one char per pixel, left to
    right. Bit (width - 1) is the left-most pixel, matching the drawing code's
    `(rowBits >> (WIDTH - 1 - col)) & 1` in addDigit()/addArrow()."""
    return "".join(
        "1" if (value >> (width - 1 - col)) & 1 else "0" for col in range(width)
    )


def check_rows(name, json_rows, literals):
    """Compare one glyph's JSON rows against its C++ row literals.
    Returns the number of mismatches found."""
    problems = 0
    for row_index, (json_row, literal) in enumerate(zip(json_rows, literals)):
        if not re.fullmatch(r"[01]+", json_row):
            print(f"FAIL: {name} row {row_index}: JSON row {json_row!r} "
                  f"contains non-binary characters")
            problems += 1
            continue
        if len(literal) != len(json_row):
            print(f"FAIL: {name} row {row_index}: row widths differ "
                  f"(C++ {len(literal)} bits vs JSON {len(json_row)} chars)")
            problems += 1
            continue
        decoded = decode_row(int(literal, 2), len(json_row))
        if decoded != json_row:
            print(f"FAIL: {name} row {row_index}:")
            print(f"  JSON   : {json_row}")
            print(f"  C++    : {literal}  (decoded {decoded})")
            problems += 1
    return problems


def extract_layout_values(cpp_text):
    """Return {constexpr name: numeric value} for the scalar `constexpr
    int/float NAME = value;` declarations in the C++ source. Symbolic values
    such as `dividerX = CANVAS_WIDTH / 2` are resolved against the other
    declarations."""
    values = {}
    declarations = re.findall(
        r"constexpr\s+(?:int|float)\s+(\w+)\s*=\s*([^;]+);", cpp_text
    )
    for name, raw in declarations:
        token = raw.strip()
        if token.isdigit():
            values[name] = int(token)
        elif re.fullmatch(r"\d+(?:\.\d+)?f", token):
            values[name] = float(token[:-1])
    for name, raw in declarations:
        half_match = re.fullmatch(r"(\w+)\s*/\s*2", raw.strip())
        if half_match and half_match.group(1) in values:
            values[name] = values[half_match.group(1)] // 2
    return values


def check_layout(cpp_text, layout_json):
    """Compare layout.json against the C++ layout constants.
    Returns the number of mismatches found."""
    if not isinstance(layout_json, dict):
        fail(f"{LAYOUT_JSON_PATH}: expected a JSON object of layout constants")
    values = extract_layout_values(cpp_text)
    problems = 0
    for json_key, cpp_name in LAYOUT_CONSTANTS.items():
        json_value = layout_json.get(json_key)
        if not isinstance(json_value, (int, float)):
            print(f"FAIL: layout '{json_key}': expected a number, got "
                  f"{json_value!r}")
            problems += 1
            continue
        if cpp_name not in values:
            print(f"FAIL: layout '{json_key}': no `constexpr {cpp_name}` in "
                  f"{RENDER_CPP_PATH}")
            problems += 1
            continue
        if float(json_value) != float(values[cpp_name]):
            print(f"FAIL: layout '{json_key}': layout.json {json_value} vs "
                  f"C++ {cpp_name} = {values[cpp_name]}")
            problems += 1
    return problems


def main():
    errors = 0

    font_json = load_json(FONT_JSON_PATH)
    if not isinstance(font_json, dict):
        fail(f"{FONT_JSON_PATH}: expected a JSON object keyed by digit")

    keys = list(font_json.keys())
    if keys != FONT_DIGITS:
        print(f"FAIL: digit order in {FONT_JSON_PATH.name}: got {keys}, "
              f"expected {FONT_DIGITS}")
        errors += 1

    cpp_text = RENDER_CPP_PATH.read_text(encoding="utf-8")
    font_literals = extract_literals(
        cpp_text, "FONT_5X7[10][7]", len(FONT_DIGITS) * FONT_ROWS
    )
    arrow_literals = extract_literals(cpp_text, "ARROW_RIGHT[7]", ARROW_ROWS)

    for digit in FONT_DIGITS:
        rows = font_json.get(digit)
        if not isinstance(rows, list) or len(rows) != FONT_ROWS:
            print(f"FAIL: {FONT_JSON_PATH.name}['{digit}']: expected a list of "
                  f"{FONT_ROWS} rows, got {rows!r}")
            errors += 1
            continue
        offset = int(digit) * FONT_ROWS
        errors += check_rows(f"digit '{digit}'", rows, font_literals[offset:offset + FONT_ROWS])

    arrows_json = load_json(ARROWS_JSON_PATH)
    arrow_rows = arrows_json.get("ARROW_RIGHT") if isinstance(arrows_json, dict) else None
    if not isinstance(arrow_rows, list) or len(arrow_rows) != ARROW_ROWS:
        print(f"FAIL: {ARROWS_JSON_PATH.name}: expected ARROW_RIGHT with "
              f"{ARROW_ROWS} rows")
        errors += 1
    else:
        errors += check_rows("ARROW_RIGHT", arrow_rows, arrow_literals)

    layout_json = load_json(LAYOUT_JSON_PATH)
    errors += check_layout(cpp_text, layout_json)

    if errors:
        print(f"\nFAILED: {errors} mismatch(es) between shared/display_assets "
              f"and {RENDER_CPP_PATH}")
        sys.exit(1)

    print(f"OK: {FONT_JSON_PATH.name} matches FONT_5X7 "
          f"({len(FONT_DIGITS)} digits x {FONT_ROWS} rows)")
    print(f"OK: {ARROWS_JSON_PATH.name} ARROW_RIGHT matches ARROW_RIGHT "
          f"({ARROW_ROWS} rows)")
    print(f"OK: {LAYOUT_JSON_PATH.name} matches the C++ layout constants "
          f"({len(LAYOUT_CONSTANTS)} constants)")
    print("Glyph and layout copies are in sync.")


if __name__ == "__main__":
    main()
