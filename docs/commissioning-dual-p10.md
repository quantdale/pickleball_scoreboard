# Commissioning the dual-P10 scoreboard

## Status and contract

Software is built and tested; **production firmware has NOT been flashed and physical scoreboard E2E has NOT passed**. See [executed verification](reports/dual-p10-software-verification.md). Work is on `feat/dual-p10-scoreboard`; accepted diagnostics are preserved separately.

One ESP32 drives two horizontal 32×16 P10 panels, logical **64×16**, brightness **12/255**. Panel 1 is controller-nearest (left logical half); Panel 2 is downstream. The accepted chain-block swap is retained; there is no within-panel mirroring or four-scan adapter. Driver 3.0.15, SHIFTREG/TYPE138, 8 MHz, `clkphase=true`, single DMA buffer.

| Signal | GPIO |
|---|---|
| R1/G1/B1 | 25/26/27 |
| R2/G2/B2 | 14/12/13 |
| A/B/C | 23/19/5 |
| LAT/OE/CLK | 4/15/16 |
| D/E | unassigned |

Scores use unchanged 5×7 glyphs scaled 2× to 10×14. One/two outer arrows indicate server 1/2; only the serving side lights them. Seam x30..33 and outer boundary stay black. Scores 0..99 fit; out-of-range scores show two dashes without altering game state. The Android preview is the same logical pixel set at 4:1.

**Gameplay decision:** approved doubles full side-out sets `serverNumber=2`, NOT standard doubles server 1. This is intentionally preserved. Ordinary play from 0-0-2 therefore remains on server 2. Server-1 rendering/transitions are software-tested with constructed states; no new BLE state-setting command was added. The owner must explicitly accept this house rule before real-match acceptance, or authorize a separate scoring-spec change.

**Known limitation:** owner-accepted suspected narrow edge/seam artifact. Do not reopen bring-up unless normal score readability or play is affected. No brightness increase, second-court implementation or new BLE features are included.

## Build, tests and deliverables

From the repository root in Git Bash:

```bash
PYTHONIOENCODING=utf-8 firmware/.venv/Scripts/pio.exe run -d firmware -e esp32dev
firmware/.venv/Scripts/python.exe scripts/run_firmware_host_tests.py --output-dir <external-session-artifacts>/host
firmware/.venv/Scripts/python.exe scripts/check_glyphs.py
firmware/.venv/Scripts/python.exe scripts/check_display_snapshots.py --cpp-tool <external-session-artifacts>/host/render-snapshots.exe --images <external-session-artifacts>/pixels

export JAVA_HOME='/c/Program Files/Android/Android Studio/jbr'
cd android
./gradlew testDebugUnitTest assembleDebug lintDebug
./gradlew connectedDebugAndroidTest # running emulator required
```

Normal firmware host testing remains `cd firmware && .venv/Scripts/pio.exe test -e native` when the native C++ toolchain works. This host's unrelated MinGW `libisl-23.dll` issue is avoided using installed MSVC; it was not repaired. The same four Unity suites run in either route. Never use bare `pio test` (it waits for real hardware) or bare `pio run` (also tries the test-only native environment).

`shared/render_fixtures/` contains 48 complete logical pixel snapshots consumed by Android JUnit. The independent Python asset oracle and actual compiled C++ exporter must both match them. Use `--update` only after an approved display change; do not regenerate fixtures merely to hide a regression.

Session/deliverables root:

```text
D:\Documents\tryPython\_AgentArtifacts\pickleball_scoreboard\sessions\20261116-pi-scoreboard\
  deliverables/firmware/{firmware.bin,firmware.elf,bootloader.bin,partitions.bin,boot_app0.bin}
  deliverables/android/pickle-scoreboard-debug.apk
  deliverables/rollback/boundary/{firmware.bin,bootloader.bin,partitions.bin,boot_app0.bin}
  artifact-manifest.json
  source-snapshot.zip
  android-runtime-verification.log
  firmware-tests.log
  preview-0-0.png / preview-10-25.png / preview-99-99.png
```

The manifest records hashes and flash offsets. `firmware.bin` is an application image at **0x10000**, not a complete-flash image; do not write it at zero. Prefer the explicit PlatformIO environment upload below. APK is debug-signed/installable, not a release/store build. No signing key is distributed.

### Host-specific Android instrumentation setup

`C:\Users\palac\.gradle\init.gradle` globally forces obsolete Flutter test runner/monitor versions, causing `PlatformTestStorageRegistry` failure. **Leave that global file untouched.** Use an isolated session `GRADLE_USER_HOME`; the verified session's `gradle-home/` has junctions to ordinary existing `caches/` and `wrapper/`, but no global init script. Set this home before the Gradle commands when reproducing instrumentation.

Successful read-only API35 emulator: `atd35`, GPU `host`, `-feature -Vulkan`, 1536 MB RAM, two cores, `-no-snapshot -no-window -no-audio`. ATD defaults to black screenshots: enable `adb -s <emulator> shell setprop debug.hwui.drawing_enabled 1` **on the session-owned emulator only**, before launching the app/tests. The session's `verify_android_runtime.py` records the full setup, direct test rerun/image collection and owned-emulator shutdown. Do not shut down someone else's emulator. This setup proves local UI rendering/control callbacks, not a BLE radio connection.

## Owner-gated deployment

### Fresh isolation confirmation — required before ANY ESP32 reset/upload

Do not infer safety from an earlier session or COM-port presence. The owner must freshly confirm:

- [ ] Panel supply is isolated/discharged; all external board/panel supplies are off.
- [ ] With everything unpowered, HUB75 input is disconnected from the ESP32; no live reconnection.
- [ ] ESP32 is powered by USB alone; **no external 5V**, panel 5V or 12V connected to the ESP32.
- [ ] Correct target board/USB port is identified. Enumeration found CH340 on COM11, location1-1; that is not fresh chip identification. Prior board MAC was `3c:8a:1f:a1:ba:28` (ESP32-D0WD-V3 revision3.1, 4MB). Verify safely after the isolation gate; never assume a stale COM number.

Only after confirmation and identity verification:

```bash
PYTHONIOENCODING=utf-8 firmware/.venv/Scripts/pio.exe run -d firmware -e esp32dev -t upload --upload-port <verified-port>
```

Capture upload hash verification and USB-only serial boot logs at115200. Expected: display initialization, 0-0/L/server2/DOUBLES state, BLE advertising and free/minimum heap diagnostics; no DMA failure, panic, watchdog or brownout. These logs still do not prove physical pixels. Close/release the serial port after capture.

### Reconnect panels safely

Remove USB again and keep supply isolated/discharged. Reconnect ESP32 → Panel1 J1 in the accepted orientation; Panel1 J2 → Panel2 J1; Panel2 J2 empty. Retain the accepted regulated **5V** panel wiring and common ground; the upstream12V supply feeds the buck converter only, never panels or ESP32 directly. Recheck polarity before energizing. ESP32 remains USB-powered. Never reconnect HUB75 live; blanking OE in application setup is not bootloader electrical protection.

Install the APK on the owner's Android phone only when that phone is connected/authorized; no physical phone was available in this software session. Accept the platform's Nearby Devices permission prompt; on older Android, location permission/services may be required for scanning. Scan for `PickleScore` and select the intended device.

## Physical E2E acceptance — all pending owner observation

Record date, board/phone IDs, firmware/APK hashes, state payloads and short videos/photos in a new session artifact folder. No checkbox below is satisfied by host tests or emulator images.

- [ ] Boot shows left0/right0, two left arrows, no right arrows, white on black at brightness12; digits are upright, readable, correctly placed across the two panels.
- [ ] Confirm normal digits/arrows do not make the accepted seam artifact interfere with readability. Do not restart boundary diagnosis for an otherwise harmless cosmetic strip.
- [ ] Android scan/discovery/connect works with required permissions. Game controls appear only after CCCD subscription **and a valid initial state**, without pressing Rally to trigger sync. Preview matches physical scores, serving side and server count.
- [ ] From a reset left/doubles state, execute the transition table below; verify both LED output and Android-reported state after each action.
- [ ] Reach left scores10,25,99 through ordinary Rally Left inputs while left serves; check spacing, clipping, seam and app equivalence. Reset afterward. This is a bench test, not an active match.
- [ ] Reset supports both starting sides and singles/doubles. Reset is one atomic three-byte command (`0LD`, `0RD`, `0LS`, `0RS`), never three separate writes.
- [ ] Disconnect/reconnect mid-game and while ended. Reconnect immediately restores current scores/side/server/mode/ended flag without resetting or requiring another rally. Background/foreground and reopening the app do not lose ESP32 state.
- [ ] Permission denial/Bluetooth-off/discovery failure shows an error without enabling controls; retry succeeds after conditions are restored. No stale callback from the old session overwrites the new session.
- [ ] Rapid but reasonable ordered button presses preserve input order; no unexpected double score, reset fragmentation, hung write queue or `command queue full` serial errors.
- [ ] Run at least20 minutes of ordinary play/control/reconnect cycles. Inspect serial free/minimum heap after initialization and under load, watch for progressive loss, resets, watchdogs/brownouts or display corruption. Build-time RAM figures do NOT cover live DMA/BLE allocations.
- [ ] Owner explicitly accepts preserved side-out-to-server2 behavior and final readability before declaring real-match readiness.

### Deterministic doubles exercise

Each row starts from the previous row; initial state is reset left/DOUBLES. Mode stays DOUBLES throughout.

| Action | Left,right,serving,server,ended |
|---|---|
| Boot/reset left | `0,0,L,2,0` |
| Rally Left | `1,0,L,2,0` |
| Rally Right (full side-out) | `1,0,R,2,0` |
| Rally Right | `1,1,R,2,0` |
| Undo | `1,0,R,2,0` |
| Undo again | unchanged (single-step undo) |
| Switch Courts | `0,1,L,2,0` |
| End Game | `0,1,L,2,1` |
| Rally Left / Rally Right / Switch | unchanged while ended |
| Undo | `0,1,L,2,0` |
| Reset right/doubles | `0,0,R,2,0` |

Separately reset left/SINGLES; Rally Right immediately switches service to right without scoring and retains server2. In production the Android app does not execute its Kotlin scoring mirror; only ESP32 Notify updates drive the preview.

## Rollback

Preserved `single` (default), `dual` and `boundary` profiles under `hardware-tests/p10-bringup/` are unchanged. Accepted boundary application SHA256:

```text
f9f194a50dd0eb989f914af49c57222f423764c94ae42741ca52c55d628e06ea
```

Its source snapshot, binary and companion boot images are retained with the deliverables. **Repeat the same fresh isolation/identity gate** before rollback, then:

```bash
PYTHONIOENCODING=utf-8 firmware/.venv/Scripts/pio.exe run -d hardware-tests/p10-bringup -e boundary -t upload --upload-port <verified-port>
```

Rollback restores a diagnostic, not scoring/BLE. Do not restore the old partial pretest flash backup as though it were a full flash: it covers only0x00000..0x5ffff. Do not change brightness, wiring, timing or gameplay to conceal a commissioning failure.
