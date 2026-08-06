#!/usr/bin/env bash
#
# verify.sh - one-stop verification for the pickleball_scoreboard repository.
#
# Usage:
#   bash scripts/verify.sh                 # run all checks
#   bash scripts/verify.sh --firmware-only # glyph drift + firmware build + host tests
#   bash scripts/verify.sh --android-only  # glyph drift + Android unit tests
#   bash scripts/verify.sh --help          # print this usage and exit
#
# Checks (each prints PASS/FAIL; the script exits non-zero if any fails):
#   1. Glyph drift check - scripts/check_glyphs.py compares the shared JSON
#      glyphs (shared/display_assets) against the firmware's embedded C++ copies
#      in firmware/src/display_render_logic.cpp.
#   2. Firmware build - `pio run -e esp32dev` from firmware/ (uses
#      firmware/.venv Scripts/pio.exe when present, else a `pio` on PATH).
#      The -e is required: bare `pio run` also builds the native env, which
#      is test-only and fails to link outside `pio test`.
#   3. Firmware host tests - `pio test -e native` from firmware/. NEVER run
#      bare `pio test`: it processes the esp32dev env first and hangs waiting
#      for hardware. The native env needs a g++ toolchain on PATH; if the step
#      fails and no g++ is found, a hint is printed (on this host:
#      export PATH="/c/msys64/mingw64/bin:$PATH").
#   4. Android unit tests - `./gradlew testDebugUnitTest` from android/. Needs
#      JAVA_HOME; if it is unset and the Android Studio bundled JBR exists at
#      '/c/Program Files/Android/Android Studio/jbr', this script sets
#      JAVA_HOME automatically.
#
# Requirements: bash; a python interpreter for step 1 (the firmware venv's
# python is preferred); PlatformIO for steps 2-3 (firmware/.venv preferred);
# the Gradle wrapper for step 4. All paths are derived from this script's
# location, so verify.sh can be invoked from any working directory.
#

set -u  # no -e: run every enabled step and report all failures

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

FIRMWARE_ONLY=0
ANDROID_ONLY=0

for arg in "$@"; do
    case "$arg" in
        --firmware-only) FIRMWARE_ONLY=1 ;;
        --android-only) ANDROID_ONLY=1 ;;
        --help|-h)
            echo "Usage: bash scripts/verify.sh [--firmware-only|--android-only]"
            echo "See the header of scripts/verify.sh for details."
            exit 0
            ;;
        *)
            echo "ERROR: unknown argument '$arg'" >&2
            echo "Usage: bash scripts/verify.sh [--firmware-only|--android-only]" >&2
            exit 2
            ;;
    esac
done

if [ "$FIRMWARE_ONLY" = 1 ] && [ "$ANDROID_ONLY" = 1 ]; then
    echo "ERROR: --firmware-only and --android-only are mutually exclusive" >&2
    exit 2
fi

# Prefer the project's PlatformIO venv; fall back to a `pio` on PATH.
find_pio() {
    if [ -x "$ROOT_DIR/firmware/.venv/Scripts/pio.exe" ]; then
        printf '%s\n' "$ROOT_DIR/firmware/.venv/Scripts/pio.exe"
    elif [ -x "$ROOT_DIR/firmware/.venv/bin/pio" ]; then
        printf '%s\n' "$ROOT_DIR/firmware/.venv/bin/pio"
    elif command -v pio >/dev/null 2>&1; then
        printf '%s\n' "pio"
    else
        printf '%s\n' ""
    fi
}

# Prefer the firmware venv's python; fall back to a `python` on PATH.
find_python() {
    if [ -x "$ROOT_DIR/firmware/.venv/Scripts/python.exe" ]; then
        printf '%s\n' "$ROOT_DIR/firmware/.venv/Scripts/python.exe"
    elif [ -x "$ROOT_DIR/firmware/.venv/bin/python" ]; then
        printf '%s\n' "$ROOT_DIR/firmware/.venv/bin/python"
    elif command -v python >/dev/null 2>&1; then
        printf '%s\n' "python"
    else
        printf '%s\n' ""
    fi
}

step_glyph_check() {
    echo ""
    echo "=== [1] Glyph drift check (shared/display_assets vs firmware) ==="
    PY="$(find_python)"
    if [ -z "$PY" ]; then
        echo "FAIL: no python interpreter found (looked in firmware/.venv and PATH)"
        return 1
    fi
    if "$PY" "$ROOT_DIR/scripts/check_glyphs.py"; then
        echo "PASS: glyph drift check"
        return 0
    fi
    echo "FAIL: glyph drift check"
    return 1
}

step_firmware_build() {
    echo ""
    echo "=== [2] Firmware build (pio run -e esp32dev, from firmware/) ==="
    PIO="$(find_pio)"
    if [ -z "$PIO" ]; then
        echo "FAIL: no PlatformIO executable found (looked in firmware/.venv and PATH)"
        return 1
    fi
    # NOTE: -e esp32dev is required; bare `pio run` also builds the native
    # env, which is test-only and fails to link outside `pio test`.
    if (cd "$ROOT_DIR/firmware" && "$PIO" run -e esp32dev); then
        echo "PASS: firmware build"
        return 0
    fi
    echo "FAIL: firmware build (see output above)"
    return 1
}

step_firmware_tests() {
    echo ""
    echo "=== [3] Firmware host tests (pio test -e native, from firmware/) ==="
    PIO="$(find_pio)"
    if [ -z "$PIO" ]; then
        echo "FAIL: no PlatformIO executable found (looked in firmware/.venv and PATH)"
        return 1
    fi
    # NOTE: -e native is mandatory; bare `pio test` builds the esp32dev env
    # first and hangs waiting for hardware.
    if (cd "$ROOT_DIR/firmware" && "$PIO" test -e native); then
        echo "PASS: firmware host tests"
        return 0
    fi
    if ! command -v g++ >/dev/null 2>&1; then
        echo ""
        echo "HINT: the native test env needs a g++ toolchain on PATH. On this"
        echo "      host, add MSYS2 MinGW, e.g.:"
        echo "          export PATH=\"/c/msys64/mingw64/bin:\$PATH\""
        echo "      then re-run this script."
    fi
    echo "FAIL: firmware host tests (see output above)"
    return 1
}

step_android_tests() {
    echo ""
    echo "=== [4] Android unit tests (./gradlew testDebugUnitTest, from android/) ==="
    if [ -z "${JAVA_HOME:-}" ]; then
        JBR="/c/Program Files/Android/Android Studio/jbr"
        if [ -d "$JBR" ]; then
            export JAVA_HOME="$JBR"
            echo "JAVA_HOME was unset; using Android Studio bundled JBR: $JBR"
        else
            echo "WARN: JAVA_HOME is unset and no Android Studio JBR found at"
            echo "      '$JBR'; set JAVA_HOME to a JDK 17+ and re-run."
        fi
    fi
    if (cd "$ROOT_DIR/android" && ./gradlew testDebugUnitTest); then
        echo "PASS: android unit tests"
        return 0
    fi
    echo "FAIL: android unit tests (see output above)"
    return 1
}

steps="step_glyph_check"
if [ "$FIRMWARE_ONLY" = 1 ]; then
    steps="$steps step_firmware_build step_firmware_tests"
elif [ "$ANDROID_ONLY" = 1 ]; then
    steps="$steps step_android_tests"
else
    steps="$steps step_firmware_build step_firmware_tests step_android_tests"
fi

failures=0
total=0
for step in $steps; do
    total=$((total + 1))
    if ! "$step"; then
        failures=$((failures + 1))
    fi
done

echo ""
echo "================================================================"
if [ "$failures" -gt 0 ]; then
    echo "SUMMARY: $failures of $total checks FAILED"
    exit 1
else
    echo "SUMMARY: all $total checks passed"
    exit 0
fi
