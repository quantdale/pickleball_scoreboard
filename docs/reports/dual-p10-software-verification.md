# Dual-P10 scoreboard: software verification

## Result

**Software ready for owner-gated flashing. Not physically commissioned.** No production ESP32 upload, serial reset, panel power-on or real BLE E2E was performed in this session. The accepted diagnostic remains the last known flashed firmware.

Worktree: `D:\Documents\tryPython\pickleball_scoreboard`, branch `feat/dual-p10-scoreboard`, based on fetched `origin/main` at `3ae7ad2e12e241e3e75e5bba1c2a78ed8bb65d4d`. Implementation is uncommitted; the external source snapshot/manifest records the exact tested contents. Existing diagnostic/OpenSpec work was preserved, not reset, stashed or committed.

## Implemented production integration

- Two horizontal32×16 panels, logical64×16, accepted pins/chain-block routing, SHIFTREG/TYPE138/8MHz/clock phase, single DMA buffer, brightness12/255. DMA-init failure halts normal operation; initial0-0-2 is drawn before BLE setup. Rendering occurs on state changes only.
- Shared unchanged5×7 digit font at2× scale, compact mirrored5×5 arrows in separate outer gutters, black seam/boundary, readable0..99 and deterministic out-of-range dashes. Firmware static data is drift-checked against JSON; Android uses those JSON assets directly.
- Firmware callback-to-loop command queue gives scoring/undo/rendering one owner. Initial/reconnect state sync occurs after CCCD enable, not prematurely on connection. Disconnect does not reset the game.
- Android waits for successful subscription **and a valid initial snapshot**, in either callback order. A bounded acknowledged-write FIFO preserves command/reset ordering; stale GATT/scan callbacks are ignored and disconnect/disposal clears resources. Parser accepts exactly six canonical fields. The actual4:1 Canvas is not stretched by its weighted parent; rasterization is explicitly black/white, without anti-aliased gray grid seams.
- Firmware `game_state.cpp` and Android `ScoreboardStateMachine.kt` are unchanged. Android production UI never runs that scoring mirror. Existing BLE name, UUIDs, command bytes, three-byte resets, six-field payload, Notify-only state and open single-central design are unchanged.

## Executed checks

| Check | Result |
|---|---|
| Actual ESP32 PlatformIO build, `-e esp32dev` | PASS; pinned platform6.13.0, HUB75 DMA3.0.15, GFX1.12.6 |
| Existing scoring Unity suite | 17/17 PASS |
| Existing BLE parser Unity suite | 22/22 PASS |
| Updated display Unity suite | 6/6 PASS; all scores0..99 across sides/servers, unique in-bounds pixels, gutters,10/25/99, overflow and chain permutation |
| New protocol lifecycle Unity suite | 6/6 PASS; CCCD sync gating, disconnect/resubscribe, exact serialization, ended/reset/undo, invalid bytes and2,000 house-rule rallies |
| Firmware host total, existing Unity through MSVC | **51/51 PASS**, no hardware |
| Shared glyph/layout drift checker | PASS;10 glyphs, arrow and12 constants |
| Independent JSON oracle vs actual compiled C++ renderer | **48 complete64×16 snapshots PASS**; both sides/servers/modes, ended states,0/1/10/25/99 and invalid display fallback |
| Android host JUnit | **42/42 PASS**: scoring17, parser18, GATT readiness/FIFO5, shared pixels2 |
| Android `assembleDebug` | PASS; installable debug-signed APK |
| Android `lintDebug` | PASS with existing baseline; not a claim of baseline-free lint cleanliness |
| Actual API35 emulator Compose instrumentation | **1/1 PASS**, also repeated directly via `am instrument` |
| Emulator renderer assertions | All1,024 logical pixel centers match expected pixels for0-0,10-25,99-99; actual4:1 bounds; every raster pixel strictly black/white; all six controls plus Disconnect dispatch the correct callback |
| Actual app launch/connection screen | PASS; `Not connected`, scan action, no premature game controls; no AndroidRuntime errors in final capture |

Host Unity bypasses this machine's unrelated MinGW/libisl failure; no native-toolchain repair was attempted. The real Xtensa build independently verifies embedded source/API compatibility. Emulator verification used a read-only `atd35` with host GPU, Vulkan disabled and ATD drawing enabled. A global Gradle legacy Flutter-test pin caused an initial instrumentation dependency failure; a session-isolated Gradle home resolved it without editing global configuration. See [reproduction instructions](../commissioning-dual-p10.md).

LSP coverage was incomplete on several C++/Kotlin paths; passing compilers/tests, not an empty diagnostics cache, are the validation evidence. The existing lint baseline still suppresses legacy findings; dependency/AGP update warnings remain. AGP8.5.1 warns that it was tested through compileSdk34 while this project targets35. No broad dependency/SDK upgrade was made to suppress that warning.

### Memory

- Static RAM: **39,292 /327,680 bytes (12.0%)**.
- Linked flash use: **1,149,869 /1,310,720 bytes (87.7%)**; existing partition scheme retained.
- These are build-time figures. DMA allocation, BLE runtime heap/minimum heap, command-load stability and long-running behavior remain hardware acceptance items. No live production heap figure is claimed.

## Deliverables and evidence

Root: `D:\Documents\tryPython\_AgentArtifacts\pickleball_scoreboard\sessions\20261116-pi-scoreboard\`.

| File | SHA256 |
|---|---|
| `deliverables/firmware/firmware.bin` | `14e77d5d48aee05652276a2757277439e89dff6d92614b76a0882294638228ae` |
| `deliverables/android/pickle-scoreboard-debug.apk` | `33046ff5d0d3f91963865cd52daf743fb9e6ae582dd1edfdfaf0e950ff1b8c00` |
| `deliverables/rollback/boundary/firmware.bin` | `f9f194a50dd0eb989f914af49c57222f423764c94ae42741ca52c55d628e06ea` |

The rollback hash exactly matches the prior accepted boundary firmware and current preserved boundary build. Companion bootloader/partition/boot_app0 images and offsets are in `artifact-manifest.json`; application binaries are NOT complete-flash images. `source-snapshot.zip` and `working-tree.patch` preserve the tested source plus existing diagnostic rollback material; no credentials, local SDK properties or signing key are packaged.

Key evidence: `firmware-final-build.log`, `firmware-tests.log`, `pixels-verification.log`, `android-runtime-verification.log`, `android-direct-instrumentation.log`, copied test/lint XML reports, `preview-0-0.png`, `preview-10-25.png`, `preview-99-99.png`, `connection-ui-final.xml/png`. Source-oracle images are separately under `pixels/`; the three `preview-*` files are **actual emulator Canvas captures**, not fabricated screenshots. The session-owned emulator was shut down after verification.

## Outstanding owner gates

CH340 enumeration currently reports **COM11**, USB VID:PID1A86:7523, location1-1. No serial port was opened; enumeration alone does not verify chip identity or electrical isolation.

1. Freshly confirm isolated/discharged panel supply, HUB75 disconnected while unpowered, ESP32 USB-only with **no external5V**, and intended board identity before reset/upload.
2. After a safe upload/reconnection, observe real pixels, readability, BLE discovery/sync, scoring/undo/end/reset/singles/doubles, app preview equality and reconnect retention. Complete the [physical checklist](../commissioning-dual-p10.md).
3. Observe a sustained real-hardware run and free/minimum heap; confirm no resets/brownouts or ordering problems.
4. Explicitly accept the preserved **nonstandard doubles side-out-to-server2** rule before real-match acceptance. The known seam artifact remains accepted unless it affects normal operation/readability.

Host/emulator success closes none of these physical gates. No second controller/court or brightness increase was implemented.
