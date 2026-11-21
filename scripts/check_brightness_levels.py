"""Check the firmware brightness ladder against the pinned HUB75 driver (Spec 03 §2a).

Models MatrixPanel_I2S_DMA::setBrightnessOE() and the BCM row repetition of
ESP32 HUB75 LED MATRIX PANEL DMA Display 3.0.15 for the installed setup:
64-clock rows (two 32x16 panels), colour depth 8, latch blanking 2 and
lsbMsbTransitionBit 0 (docs/reports/p10-physical-color-isolation.md). The
light from a lit white LED is proportional to its output-enable time.

Usage: python scripts/check_brightness_levels.py
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "firmware/include/display_brightness.h"
PLATFORMIO = ROOT / "firmware/platformio.ini"
WIDTH, DEPTH, BLANK, TRANSITION = 64, 8, 2, 0
EXPECTED_MULTIPLES = [1, 2, 3, 4, 6, 8, 12, 16]


def repeats(index):
    return 1 + ((1 << (index - TRANSITION - 1)) if index > TRANSITION else 0)


def oe_window(index, value):
    """Enabled clocks of one row for colour bit `index` (driver lines ~690-725)."""
    bitplane = (2 * DEPTH - index) % DEPTH
    bitshift = (DEPTH - TRANSITION - 1) >> 1
    rightshift = max(bitplane - bitshift - 2, 0)
    maximum = (WIDTH - BLANK) >> rightshift
    pixels = (maximum * value) >> 8
    if value > 0 and pixels == 0:
        pixels = 1
    if pixels > maximum - 1:
        pixels = maximum - 1
    return ((WIDTH + pixels + 1) >> 1) - ((WIDTH - pixels) >> 1)


def light(value):
    """Output-enable clocks per row address for a fully lit white LED."""
    return sum(repeats(i) * oe_window(i, value) for i in range(DEPTH))


def main():
    errors = []
    if "ESP32 HUB75 LED MATRIX PANEL DMA Display@3.0.15" not in PLATFORMIO.read_text():
        errors.append("driver pin changed: re-derive this model before trusting the ladder")
    text = HEADER.read_text()
    match = re.search(r"BRIGHTNESS_LADDER\[\]\s*=\s*\{([^}]*)\}", text)
    ladder = [int(v) for v in match[1].split(",")] if match else []
    cap = re.search(r"#define PICKLE_BRIGHTNESS_MAX_LEVEL (\d+)", text)
    if sum(repeats(i) for i in range(DEPTH)) != 135:
        errors.append("model no longer matches the 135-transfer schedule")
    if len({light(v) for v in range(1, 13)}) != 1:
        errors.append("values 1..12 no longer share the minimum output")
    if light(13) <= light(12):
        errors.append("13/255 is not brighter than 12/255")
    if not ladder or ladder[0] != 12:
        errors.append(f"level 1 must stay 12/255, found {ladder[:1]}")
    if ladder and max(ladder) > 128:
        errors.append("ladder exceeds the 128/255 ceiling")
    base = light(12)
    print("level value  OE clocks/row  x level 1")
    for level, value in enumerate(ladder, 1):
        ratio = light(value) / base
        print(f"{level:5d} {value:5d} {light(value):14d} {ratio:10.2f}")
        if level > 1 and light(value) <= light(ladder[level - 2]):
            errors.append(f"level {level} is not brighter than level {level - 1}")
        expected = EXPECTED_MULTIPLES[level - 1] if level <= len(EXPECTED_MULTIPLES) else None
        if expected is None or abs(ratio - expected) > 0.15 * expected:
            errors.append(f"level {level} gives x{ratio:.2f}, documented x{expected}")
        # A value at a plateau edge would change with a one-step rounding difference.
        if light(value - 1) != light(value) and light(value + 1) != light(value):
            errors.append(f"level {level} value {value} sits on a single-value plateau")
    if not cap or not 1 <= int(cap[1]) <= len(ladder):
        errors.append("default PICKLE_BRIGHTNESS_MAX_LEVEL must select a ladder level")
    else:
        print(f"default cap: level {cap[1]} (x{light(ladder[int(cap[1]) - 1]) / base:.2f}), uncertified")
    for error in errors:
        print("ERROR:", error, file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
