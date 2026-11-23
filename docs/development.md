# Develop and verify PickleScore

All commands here are build/test only. No serial access, phone installs or
Court 1 maintenance is required.

## Code map

| Path | Responsibility |
|---|---|
| `firmware/src`, `firmware/include` | Arduino main loop owns scoring, the 32-position journal, display settings and BLE dispatch; OTA maintains a separate, signed inactive-slot transaction. |
| `android/app/src/main` | The Compose controller renders only notified state. `ble/` owns framing, readiness, timeouts and acknowledged FIFO writes; `ui/` owns the landscape layout and pixel preview. |
| `firmware/test`, `android/app/src/test`, `android/app/src/androidTest` | Unity, JVM and Compose regression suites. Kotlin's state machine is a test oracle, never production logic. |
| `shared/` | Glyphs/layout, full pixel snapshots and shared scoring traces. The C++ renderer embeds copies of the asset constants; drift and pixel checks protect parity. |
| `scripts/`, `tools/` | Build/pixel/package audits, offline tests, Windows deployment CLI and isolated recovery helpers. |
| `hardware-tests/p10-bringup` | Standalone spare-only panel diagnostics. |
| `docs/specs`, `docs/reports`, `openspec` | Current contracts, dated evidence, and tracked planning/archives. |

## Firmware and host tools

Install Python 3.11+ and a host C++ compiler, and install PlatformIO Core
6.1.19 into a virtual environment (CI uses the same version). The explicit
`espressif32@6.13.0` platform and HUB75/GFX library pins live in
[`firmware/platformio.ini`](../firmware/platformio.ini). Use OpenSSL for signing
tests. From the repository root with the venv active:

```bash
pio run -d firmware -e esp32dev -e ota -e ota_b
pio test -d firmware -e native
python scripts/check_glyphs.py
python scripts/check_brightness_levels.py
python scripts/check_scoring_build.py
python -m unittest discover -s tools -v
# Choose a new directory outside the checkout; these signing keys are test-only.
python scripts/test_ota_tool.py --output-dir "$SESSION/signing-tests"
# Compile the C++ exporter and compare it with the independent asset oracle.
c++ -std=c++11 -I firmware/include scripts/render_snapshots.cpp \
  firmware/src/display_render_logic.cpp -o "$SESSION/render-snapshots"
python scripts/check_display_snapshots.py --cpp-tool "$SESSION/render-snapshots"
```

Set `SESSION` to an existing external evidence directory. On Windows use `.exe`
for the exporter. If native MinGW cannot run, install the `native` Unity
library with `pio pkg install -d firmware -e native`, then run
`python scripts/run_firmware_host_tests.py --output-dir "$SESSION/host"` with
MSVC installed. The script also builds `render-snapshots.exe`. Never use bare
`pio test`. `ota_budget`, `ota_ownership` and `ota_rollback_test` are diagnostics,
not production builds. `esp32dev` is not an OTA update image. For the optional
rollback/partition audit, `scripts/check_ota_build.py` requires the preserved
public pre-OTA baseline directory, not a freshly rebuilt substitute.

## Android

Install JDK 17 and Android SDK Platform 35. The repository uses Gradle 8.9,
AGP 8.5.1 and Kotlin 1.9.25; versions are pinned in the wrapper and app build.
Use the SDK from Android Studio or set `ANDROID_HOME` to it.

```bash
cd android
bash ./gradlew testDebugUnitTest lintDebug assembleDebug assembleDebugAndroidTest
# Only with a session-owned emulator running and no physical device selected:
ANDROID_SERIAL=emulator-5554 bash ./gradlew connectedDebugAndroidTest
```

Use the actual serial of your emulator in place of `emulator-5554`. On the
Windows development host, isolate `GRADLE_USER_HOME` to avoid the global init
script that pins unrelated test libraries. The API 35 ATD needs drawing enabled
for screenshots (`adb -s <emulator> shell setprop debug.hwui.drawing_enabled 1`).
See the [historical emulator setup](commissioning-dual-p10.md#host-specific-android-instrumentation-setup)
for the full setup. A successful instrumentation APK build is not a test run;
Compose instrumentation does not exercise a physical BLE radio.
