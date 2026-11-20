# AGENTS.md

This file is intended for AI coding agents working on the `pickleball_scoreboard` project.

## Project overview

A **pickleball scoreboard** system with two implementations driven by one shared state:

- **ESP32 firmware** (`firmware/`) — drives two horizontal 32×16 P10 HUB75 panels (64×16), owns the authoritative game state, and acts as a BLE peripheral. Brightness is fixed at 12/255.
- **Android app** (`android/`) — a Jetpack Compose remote control and live preview. It is a "dumb renderer": it sends button-press commands over BLE and displays whatever state the ESP32 reports back. It never computes scoring logic.

**Court 1:** 16×64 visible pixels on unchanged 64×16 DMA. Last authenticated build is `doubles-serve-001` / `ota_0` / `VALID`, confirmed and healthy after the [installed wireless OTA](docs/reports/court1-doubles-serve-install.md). `portrait-order-004` on `ota_1` is the expected fallback. Both OTA connections used a temporary static IPv4 address after APIPA; BLE initial sync and Windows restoration passed. Physical doubles scoring acceptance remains separate. Do not use `portrait-installed-003` on Court 1. See `docs/portrait-operator-guide.md`. The OTA checkpoints below are historical. The one-time approval is consumed: do not activate maintenance, deploy or run scoring tests in a new session without authorization.

**Latest Court 1 checkpoint:** [`court-undo-003`](docs/reports/court1-undo-install.md) is installed on `ota_0`, independently authenticated as VALID/confirmed/healthy with the exact signed image digest. `court-controller-002` on `ota_1` was not targeted and is the expected fallback. Authenticated candidate maintenance reported 56,036 B current free internal heap and a 30,708 B largest internal byte block. Close, BLE initial sync and Windows restoration passed. The single-write authorization is consumed. No scoring commands; owner gameplay/Undo acceptance is pending.

**Pre-install readiness:** [Court 1 BLE and OTA readiness](docs/reports/court1-undo-ota-readiness.md) verified the previous `court-controller-002` source without writing. The discovery miss did not reach activation; the CLI now has a bounded passive fallback after a clean active miss.

### Authoritative specs

Behavior is defined by **approved** specs in `docs/specs/` — these are the single source of truth, and both implementations must follow them faithfully:

| Spec | Defines |
|---|---|
| `docs/specs/01-scoring-state-machine.md` | Game state fields, six inputs (rally won L/R, undo, switch courts, end game, reset), transition rules, full-session Undo, 0-0-2 game start |
| `docs/specs/02-ble-protocol.md` | BLE GATT structure (one service, Write-only command characteristic, legacy and extended Notify-only state characteristics), ASCII command format (single-byte commands; reset is a 3-byte sequence, §4a), comma-separated state payload, device name `"PickleScore"` |
| `docs/specs/03-display-rendering.md` | 16×64 portrait canvas, corrected installed panel order, white on black, frozen Doubles geometry and continuous Singles composition |
| `docs/specs/04-local-browser-ota.md` | Opt-in SoftAP/browser maintenance, private provisioning, signed inactive-slot OTA, BLE priority, rollback and explicit initial-migration authorization |

**OTA feature:** `feat/browser-softap-ota` preserves the working `feat/dual-p10-scoreboard` baseline. Profiles `ota`/`ota_b` build signed-package-capable production firmware with two0x1f0000 slots; owner-approved initial USB migration/provisioning is installed as `ota-v1-a` in `ota_0` (pending0/UNDEFINED). Full4MB backup, exact private trust record readback and all four written regions are verified; no bootloader/eFuse changes. AP-start heap~59.8KB, post-idle closure~104–107KB. Repeated SDK `wifi:timeout when WiFi un-init, type=4` remains unresolved; do not call diagnostics entirely clean or seal on startup evidence. See `docs/reports/browser-softap-ota-installation.md`. See `docs/ota-maintenance.md` and `docs/reports/browser-softap-ota-verification.md`. Never infer fresh electrical isolation or partition-migration approval from the earlier production upload. No default credentials/private signing trust; generate owner files outside Git. The current owner confirms baseline initial display/both panels work; OTA/RF/heap/rollback execution still needs commissioning.

**Agent deployment follow-up:** `feat/agent-wireless-deployment` adds `tools/ota_deploy.py`, Windows WLAN automation and HMAC BLE activation (`...26aa`, Spec04 §8). Current hardware is explicitly USB-app-installed **agent-v2-a/ota_0**, metadataUNDEFINED/pending0; no new migration/provisioning/bootloader writes. A discovered1KB cleanup-task stack overflow was corrected to4KB; full idle expiry + three authenticated reactivation/scoring cycles passed strict raw-log checks with2.4KB+ stack margin. **WPA authentication stalls on this Windows adapter before DHCP/HTTP**, including independent netsh; no wireless firmware transfer/install is proven. SDK shutdown type4/5 warnings remain unresolved. See `docs/agent-ota-deployment.md` and `docs/reports/agent-wireless-deployment-verification.md`. The earlier OTA paragraph is historical initial-install evidence. Reuse protected existing owner files; do not regenerate keys or hide a USB fallback. 77 firmware/29 CLI/15 signing/42 Android tests and48 snapshots pass; physical panel/rollback/load acceptance remains pending.

**Historical agent-wpa1 validation:** after fresh owner isolation/application-only authorization, signed `agent-wpa1` was USB-installed in `ota_0`; image and every unrelated flash sector verified. **OTA/AP startup self-check fails and disables maintenance**. Logged minimum48,208 is NOT the current-free measurement used for the unchanged49,152-byte admission policy; the exact rejected API/check was not captured. Ordinary display-driver/BLE/scoring report ready. Stopped as requested: no WPA/DHCP/HTTP test, further flash or automatic restoration. Do not claim the TX-cache candidate fixes WPA or reduce safeguards. Current recovery/fallback preserved; see `docs/reports/agent-wpa1-installation.md`. Prior agent-v2-a paragraph is historical. Further hardware writes/restoration require explicit owner authorization.

**Historical heap-correction software checkpoint:** signed `agent-wpa2` was prepared, not yet installed at that checkpoint. OTA builds release only unused Classic controller RAM before initializing BLE-only once at boot; the installed ELF reserves15,448 gross bytes for Classic. Keep cache4 and all existing free-heap floors, add current/minimum/byte/contiguous stage telemetry and additional HTTP/sector-block admission fences. Actual reclaimed heap, AP startup, repeated lifecycle and WPA remain unverified pending fresh isolation and candidate-specific app-only permission.86 firmware/29 CLI/15 signing/42 rerun Android tests and48 snapshots pass; all profiles/rollback linkage audited. See `docs/reports/ota-heap-admission-correction.md`; use its exact `candidate-final/` hashes, not preliminary outputs. No push/merge without authorization.

**Earlier hardware checkpoint (2026-11-18):** fresh owner isolation/candidate permission used for signed `agent-wpa2` app-only installation into `ota_0`; image/all other sectors independently verified, including a post-runtime whole-device recovery check. Measured Classic release net+14,968 bytes; AP-ready current67,540/min67,436/largest INTERNAL8 block47,092, original floors unchanged. Full180.02s timeout, nine authenticated BLE reactivations, five score/undo cycles and **four actual Windows WPA2/CCMP/DHCP/admin-HTTP sessions** pass; Ethernet internet and Wi-Fi state restored. SDK warnings localize in actual ELF to timer dispatch while Wi-Fi not initialized; warning and small unaccounted heap drift (warm-ready−748 bytes) remain, so no leak-free/endurance/load/panel/rollback-execution acceptance. Current USB metadata remains UNDEFINED/confirmed=false, not an OTA VALID trial. Left maintenance closed, BLE sync verified, COM11 released. See `docs/reports/agent-wpa2-hardware-verification.md`; older paragraphs above are historical. Protected current4MB recovery SHA181dcc684f706a7021cd7e39c31f2ccb226ae050b35056bbd4de8243a7a76b2f. **Signed agent-ota1 prepared only; no upload/staging/install or further update/metadata permission. Do not deploy it merely on resume.** No push/merge.

**Latest lifecycle checkpoint (2026-11-18):** installed app remains `agent-wpa2`/`ota_0`; this campaign performed no reset, firmware install, flash erase or metadata change.50 complete recorded BLE/WPA/DHCP/admin-HTTP/cleanup cycles plus one extra raw-confirmed cycle, but **not50 uninterrupted passes**: retain one real HTTP-login transport failure and one serial-interleaving parser failure. Pre-network heap120,420→116,364;300s closed quiescence recovered only128 bytes; last30 slope−68.89 bytes/cycle. No convincing plateau or proven allocation owner. SDK late-dispatch warning is not proof of a public deinit timeout; armed-delete rejection in legacy ETS is a candidate, not a confirmed cause/fix. Nine genuine signed/invalid/interrupted pre-erase upload probes,179.96s timeout and final BLE score/undo pass; no valid full image prefix or install call.88 firmware/29 CLI/15 signing/42 rerun Android tests,48 snapshots, all profiles/link audits pass; earlier C++ LSP timeouts retained, final changed-C++ probes completed clean. Source-only signed **agent-life1** adds task/allocator/delete-call/settled metrics, **not installed and not a leak fix**. Frozen agent-ota1 remains authentic but NOT lifecycle-cleared. See `docs/reports/ota-lifecycle-endurance.md` for exact package/command and protected session. Recovery copies hash181dcc684f706a7021cd7e39c31f2ccb226ae050b35056bbd4de8243a7a76b2f; fresh file verification only, no new physical full read. **Stop for explicit owner permission before any first diagnostic Wi-Fi installation, inactive-slot erase/write or normal OTA metadata changes. Resume is not permission.** No USB fallback, push or merge; retain open6.5/6.8 and lifecycle-fix/transfer/rollback/panel gates.

**Resource attribution follow-up (2026-11-18):** the most recent no-write session on `agent-wpa2` returned available INTERNAL heap from57,892 to115,972 bytes (116,276 pre-network). It did not measure a settled sample or attribute the historical4KB drift. Fixed stale authenticated status progress with an offline red/green regression. Signed bench-only `agent-life2-own` and `agent-life2-budget` add ownership and transfer/boot resource telemetry; neither was uploaded or installed. No memory/stack sizing or SDK teardown changes. See `docs/reports/ota-resource-attribution.md` for the bounded attribution matrix and backups/USB recovery. **Do not install using Court1 OTA: a valid upload would overwrite the original fallback even without install.** Preserve it; any app-only diagnostic install into the running slot needs separate permission and fresh USB isolation. Full OTA remains bench-only under this constraint.

**Last authenticated Court 1 state (2026-11-18):** The explicitly authorized first wireless bench update **PASS**: `agent-wpa2/ota_0` → signed `agent-life2-budget/ota_1`, exact image hash verified, `VALID`/confirmed/healthy. One upload/install/reset; BLE initial sync and Windows WLAN/Ethernet restored. The original on-device fallback was overwritten as authorized; it remains in the verified pre-update backups, which are not snapshots of the new installed state. `ota_0` was not targeted. A type-4 Wi-Fi warning and one armed delete rejection in `ets_timer_done` were observed; persistent heap attribution, upload task watermark, repeat-update/rollback and lit-panel acceptance remain open. See `docs/reports/ota-first-wireless-update.md`. Authorization is consumed: no additional update, forced rollback or recovery write without new approval.

**Latest Court 1 result (2026-11-18):** one separately authorized return-slot update replaced `agent-wpa2` in `ota_0` with signed `court1-return-002`. Device readback and install acknowledgment passed; UART shows `PENDING_VERIFY` and confirmation. The external guard misclassified expected `0xc (SW_CPU_RESET)` and stopped; the corrected classifier is covered by offline tests. The later AP-readiness-gated no-write session saw the exact BSSID before its single join and independently verified the exact running digest and `VALID`/confirmed/healthy on `ota_0`. Close was acknowledged and the link dropped. A later single BLE-only check verified initial sync, normal disconnect, and resumed advertising. The second OTA and operational recovery passed; AP shutdown evidence is consistent but not independently radio-verified. HTTP upload/readback watermark was 3,680 B. See `docs/reports/ota-second-wireless-update.md`. Both write authorizations are spent. **Do not rerun deployment; only seek no-write verification approval.** Source fixes to AP stop ordering and cleanup task ownership are not proven timer/heap fixes.

When implementing anything, cite the spec section in code comments (the existing code already does this, e.g. `// Spec 01 Section 4`).

## Repository layout

```text
.
├── README.md                     # Build, commissioning and verification links
├── AGENTS.md                     # This file
├── docs/
│   └── specs/                    # Authoritative behavior specs 01–03 (APPROVED)
├── firmware/                     # ESP32 firmware (PlatformIO, Arduino framework)
│   ├── platformio.ini            # env:esp32dev (HUB75 + Adafruit GFX lib_deps) + env:native host test env
│   ├── include/                  # game_state.h, ble_command_parser.h, ble_service.h, display_render_logic.h, display_render.h
│   ├── src/                      # main.cpp + module implementations
│   ├── test/                     # Four Unity suites: scoring, parser, display, protocol lifecycle
│   └── .venv/                    # Local Python venv containing PlatformIO (pio)
├── android/                      # Android app (Gradle, Kotlin, Jetpack Compose)
│   ├── settings.gradle.kts       # rootProject "PickleballScoreboard", :app
│   ├── build.gradle.kts          # Top-level (config lives in app module)
│   ├── gradle.properties
│   ├── gradlew / gradlew.bat     # Gradle 8.9 wrapper
│   └── app/
│       ├── build.gradle.kts      # AGP 8.5.1, Kotlin 1.9.25, Compose BOM 2024.06.00; adds ../../shared/display_assets as assets srcDir
│       └── src/
│           ├── main/
│           │   ├── AndroidManifest.xml   # BLE permissions, label "PickleScore"
│           │   └── java/com/example/pickleballscoreboard/
│           │       ├── MainActivity.kt
│           │       ├── ble/BleClient.kt      # BLE client: UUIDs, command bytes, scan/connect/subscribe/write
│           │       ├── state/                # ScoreboardState.kt + ScoreboardStateMachine.kt
│           │       └── ui/                   # ScoreboardScreen.kt + DisplayRenderer.kt
│           └── test/java/com/example/pickleballscoreboard/
│               ├── ble/StatePayloadParserTest.kt
│               └── state/ScoreboardStateMachineTest.kt
├── shared/
│   └── display_assets/           # Glyph + layout data shared by firmware and app (Spec 03 §6)
│       ├── font_5x7.json         # Digits 0–9 as 5×7 bit patterns
│       ├── layout.json           # 16×64 canvas, digit bands and bottom arrow
│       └── arrows.json           # ARROW_RIGHT only (5×5); left is derived by mirroring rows
├── openspec/                     # OpenSpec planning framework (schema: spec-driven)
│   ├── config.yaml
│   ├── changes/                  # Active changes + archive/
│   └── specs/                    # Main capability specs (shared-display-assets, scoring-state-machine-tests, ble-protocol)
├── .claude/ .codex/ .cursor/ .kimi/ .opencode/   # Duplicated OpenSpec agent tooling
└── firmware/.pio/                # PlatformIO build output (generated, do not edit)
```

## Technology stack

- **Firmware:** C++, PlatformIO Core 6.1.19, `framework = arduino`, board `esp32dev` (ESP32-WROOM-32). Libraries: pinned `mrfaptastic/ESP32 HUB75 LED MATRIX PANEL DMA Display@3.0.15` and `adafruit/Adafruit GFX Library@1.12.6` for the LED panels, and the Arduino `BLEDevice` stack for BLE. Serial monitor at 115200 baud.
- **Android:** Kotlin 1.9.25, Android Gradle Plugin 8.5.1, Jetpack Compose (BOM 2024.06.00, compiler extension 1.5.15), `minSdk = 26`, `compileSdk/targetSdk = 35`, JVM target 1.8. Gradle wrapper 8.9.
- **Shared data:** plain JSON glyph + layout files, parsed at runtime by the Android app only (no code generation); the firmware keeps hand-transcribed copies in `display_render_logic.cpp` (see Shared below).
- **Planning:** OpenSpec CLI with the `spec-driven` schema.

## Build and test commands

### Firmware (PlatformIO)

A working PlatformIO install lives in the project's own venv at `firmware/.venv`:

```bash
cd firmware
.venv/Scripts/pio.exe run -e esp32dev    # build (Windows; use .venv/bin/pio on POSIX)
.venv/Scripts/pio.exe run -e esp32dev -t upload --upload-port <verified-port> # ONLY after fresh USB-only isolation confirmation
.venv/Scripts/pio.exe device monitor   # serial monitor, 115200 baud
.venv/Scripts/pio.exe test -e native   # eight hardware-free Unity suites (109 tests)
.venv/Scripts/pio.exe run -e ota -e ota_b # build only; NEVER generic first-migration upload
```

(Any globally installed `pio` works too.) The `-e esp32dev` is required for builds: bare `pio run` also builds the `native` env, which is test-only and fails to link outside `pio test`. The firmware is known to compile cleanly with `pio run -e esp32dev`.

**WARN:** always pass `-e native` when running firmware tests. Bare `pio test` processes the `esp32dev` env first and hangs waiting for hardware (documented in `firmware/pio_test_debug.log`). The `native` env needs a `g++` toolchain on PATH — MSYS2 MinGW g++ exists on this host at `/c/msys64/mingw64/bin`.

If native MinGW hits the earlier `libisl-23.dll` failure, run `firmware/.venv/Scripts/python.exe scripts/run_firmware_host_tests.py --output-dir <external-session-artifacts>/host` from the repository root using installed MSVC. See commissioning docs for cross-language snapshots.

### Android (Gradle)

```bash
cd android
./gradlew assembleDebug          # build debug APK
./gradlew testDebugUnitTest      # 43 pure host JUnit tests
./gradlew connectedDebugAndroidTest # actual Compose pixels/controls on a running emulator
```

**NOTE:** this host has no JDK on PATH and `JAVA_HOME` unset. In Git Bash, point Gradle at the Android Studio bundled JBR first: `export JAVA_HOME="/c/Program Files/Android/Android Studio/jbr"`.

Unit tests need no emulator or hardware; the additional Compose instrumentation test needs an emulator. This host's global `~/.gradle/init.gradle` forces obsolete Flutter test libraries: use an isolated session `GRADLE_USER_HOME` for instrumentation, leaving that global file untouched. ATD screenshots require drawing enabled; successful setup and evidence are documented in commissioning docs. CI runs host tests and builds; real panel/BLE acceptance remains manual.

### OpenSpec (planning)

```bash
openspec new change "<name>"
openspec status --change "<name>" --json
openspec instructions <artifact> --change "<name>" --json
openspec list --json
openspec validate "<name>" --strict --no-interactive
openspec archive --change "<name>"
```

## Architecture and module division

### Firmware (`firmware/`)

Production modules are wired together in `src/main.cpp`; OTA is opt-in at compile time. The three hardware-free modules (`game_state`, `ble_command_parser`, `display_render_logic`) are the ones covered by the `native` test env's `build_src_filter` in `platformio.ini`:

- `game_state.h/.cpp` — `GameState` owns the current position, internal doubles opening flag, and a compact in-RAM Undo journal (Spec 01 §§4–5a). The opening team starts on server 2; all doubles side-outs give serve to the opponent at server 1. `handleReset` takes `(GameState&, Side startingSide, GameMode mode = GameMode::DOUBLES)` (Spec 01 §9, Spec 02 §4a).
- `ble_command_parser.h/.cpp` — pure parser `handleBleCommand(const std::string& value, GameState& state)`: maps the Spec 02 §4 ASCII command bytes (including the 3-byte reset sequence) to the `game_state` handlers. No I/O, unit tested on the host.
- `ble_service.h/.cpp` — GATT server setup per Spec 02 §3: service `4fafc201-1fb5-459e-8fcc-c5c9c331914b`, command characteristic `...26a8` (Write), legacy state `...26a9` and extended state `...26ab` (Notify), advertised as `"PickleScore"`. Callbacks enqueue bounded command records; only the Arduino loop runs `handleBleCommand`. CCCD enable requests initial sync; connect alone never notifies. `ble_sync_gate.h` and `state_payload.h` are host-tested.
- `display_render_logic.h/.cpp` — pure `renderPortraitPixels` streaming lit coordinates for the 16×64 canvas without a framebuffer or heap allocation. Deliberately embeds hand-transcribed glyph arrays and layout constants instead of parsing the shared JSON at runtime — an approved deviation from Spec 03 §6 documented in the header comment of `src/display_render_logic.cpp`.
- `display_render.h/.cpp` — guarded DMA initialization, explicit accepted pins/SHIFTREG/TYPE138/8MHz/clock phase, two 32×16 panels, brightness 12. `p10_hardware.h` maps viewer top and bottom onto raw DMA x63–32 and x31–0 respectively, preserving both local axes. Draws only on state changes; no virtual/four-scan remapping.

- `ota_maintenance.h/.cpp` — opt-in local AP/HTTP/provisioning and deferred boot-confirmation adapter. `ota_core.h` contains the same policy/auth/request/header/stream transaction and status projection exercised by22 host tests; `ota_credentials.h` fixes the record format and `ota_web.h` contains offline HTML/JS. `partitions_ota.csv` is an explicit owner-gated migration, not a drop-in ordinary upload. `scripts/ota_tool.py` handles private provisioning/signing; `scripts/check_ota_build.py` inspects/packages public builds without serial access.

### Android (`android/`)

Package `com.example.pickleballscoreboard`:

- `MainActivity.kt` — hosts `ScoreboardScreen()` and wires its button handlers to `BleClient`.
- `ble/BleClient.kt` — fully implemented: scan/connect/subscribe/write. Holds the Spec 02 UUIDs and command byte constants (`'L'`, `'R'`, `'U'`, `'C'`, `'E'`, `'0'`; reset is the **three-byte** sequence `'0'` + `'L'|'R'` + `'D'|'S'`, Spec 02 §4a amended for SINGLES). Deliberate: `CMD_RESET_SIDE_LEFT`/`CMD_RESET_SIDE_RIGHT` intentionally share byte values with `CMD_RALLY_WON_LEFT`/`CMD_RALLY_WON_RIGHT` but are kept as distinct constants (comment in the file explains why).
- `state/ScoreboardState.kt` + `state/ScoreboardStateMachine.kt` — state model (`Side`, `GameMode`, `ScoreboardState`) and a Kotlin mirror of the Spec 01 state machine (including SINGLES), covered by `ScoreboardStateMachineTest`. The Kotlin scoring mirror lives in test sources only. The legacy/extended Notify parser `parseStatePayload` lives in `ble/StatePayloadParser.kt`. `ble/GattSession.kt` gates readiness on subscription plus initial valid state and serializes acknowledged command writes; both are covered by JUnit.
- `ui/ScoreboardScreen.kt` — Android 1.3 restores the 1.1 layout: exact 16×64/1:4 LED preview on the left, stacked controls on the right, Board/version menu above. Gameplay is gated on BLE synchronization. See the [Android 1.3 release report](docs/reports/android-1.3.0.md) for the signed artifact and installed-APK verification.
- `ui/DisplayRenderer.kt` — draws the reported state using pure `ScoreboardPixels.kt`, shared JSON and the exact pixel sets exercised by JUnit and actual emulator capture tests.

Assets wiring: `app/build.gradle.kts` adds `../../shared/display_assets` as an assets `srcDir` so the glyph and layout JSON ship in the APK (Spec 03 §6).

### Shared (`shared/display_assets/`)

- `font_5x7.json` — digits `0`–`9`, each an array of 7 strings of 5 chars, `1` = lit.
- `arrows.json` — `ARROW_RIGHT` only (5 strings of 5 chars); derive left by reversing each row, never author a second bitmap.
- `layout.json` — 16×64 canvas, portrait score and server bands, anisotropic digit scale and one bottom arrow. Doubles keeps a dark seam; Singles can render its opponent score through rows 31–32.
- `../render_fixtures/` — 72 complete versioned pixel snapshots and CSV inputs. `scripts/check_display_snapshots.py` compares the independent JSON oracle with actual C++ output; Android JUnit consumes the same snapshots.

The JSON files are the **design-time source of truth** for glyphs and layout. Only the Android app parses them at runtime (via the assets `srcDir` wiring above). The firmware compiles hand-transcribed copies in `src/display_render_logic.cpp` — an approved deviation from Spec 03 §6 (documented in that file's header comment) — so any glyph or layout change must be mirrored into the C++ arrays/constants manually; `scripts/check_glyphs.py` verifies the copies stay in sync.

## OpenSpec workflow conventions

This repo follows the OpenSpec `spec-driven` schema defined in `openspec/config.yaml`:

- A **change** is created with `openspec new change "<kebab-case-name>"` and lives under `openspec/changes/<change-name>/` with `proposal.md`, `design.md`, `tasks.md`, and optional delta specs under `specs/`.
- Main capability specs live at `openspec/specs/<capability>/spec.md`. Three exist: `shared-display-assets`, `scoring-state-machine-tests`, and `ble-protocol` (the last is part of the uncommitted singles-mode work). The `docs/specs/` files remain the authoritative behavior specs.
- Completed changes are archived under `openspec/changes/archive/YYYY-MM-DD-<change-name>/`.

### Rules for OpenSpec artifacts

- Always read `openspec status --change "<name>" --json` and `openspec instructions <artifact> --change "<name>" --json` before creating or editing artifacts.
- Use the template and instructions returned by the CLI; the `context` and `rules` fields are constraints for you — do **not** copy them into the artifact.
- Read dependency artifacts before producing a new one.
- Task checkboxes in `tasks.md` use `- [ ]` / `- [x]`; mark tasks complete immediately after finishing them.
- Change names are `kebab-case`.

### Active changes

(Check `openspec list --json` for current progress.)

- `openspec/changes/agent-wireless-deployment/` — both signed wireless updates reached independently authenticated `VALID` on opposite slots; operational post-maintenance BLE recovery passed. No further writes are authorized. Rollback, sustained load, and owner-observed lit-panel acceptance remain open; do not merge.

- `openspec/changes/browser-softap-ota/` — approved initial migration/private provisioning and USB-only startup complete. Actual browser wireless updates, rollback execution, real BLE+display+load stability and the unresolved SDK idle-shutdown warning remain acceptance gates. Keep owner secrets/full backups outside Git; any later physical serial work needs fresh isolation if wiring changes. No push/merge without authorization.

- `openspec/changes/commission-dual-p10-scoreboard/` — production integration/builds/tests and actual USB-only upload/serial boot verification complete; only owner-observed physical hardware E2E remains. Do not revive old 64×32 commissioning instructions.
- `openspec/changes/p10-hardware-bringup/` and `hardware-tests/p10-bringup/` — preserved diagnostic/rollback work; bring-up is owner-accepted. Do not reopen the known narrow seam artifact unless normal score readability/play is affected.

- `openspec/changes/implement-ble-wiring/` — implementation complete (41/42 tasks); only the manual hardware E2E task remains (`manual-e2e-checklist.md`).
- `openspec/changes/implement-display-rendering/` — code done; remaining tasks are build/visual verification and manual hardware validation (`manual-e2e-checklist.md`).
- `openspec/changes/scaffold-firmware-and-app-2/` — complete (13/13 tasks) but never archived.

Archived: `2026-07-22-scaffold-firmware-and-app`, `2026-07-22-implement-scoring-state-machine`, and `2026-07-23-implement-singles-mode` (SINGLES mode — the archived change directory is currently **untracked/uncommitted** in the working tree; see Agent-specific notes).

Available agent workflows (slash commands / skills, duplicated per IDE): `propose`, `apply`, `update`, `sync`, `archive`, `explore`.

## Code style guidelines

- **Match the existing style of the file you're editing.** Firmware uses 4-space indent, Allman-ish braces for functions, `#ifndef` header guards, and spec-section citations in comments. Android code is standard Kotlin with `kotlin.code.style=official`.
- **Comment convention:** reference the governing spec and section (e.g. `// Spec 02 Section 4a`) wherever behavior is spec-defined. Keep comments factual and sparse, like the existing code.
- **Specs are authoritative over code and over the old HTML mockup** (Spec 03 Note A: never port the mockup's JS game logic; Spec 01 rules scoring).
- **Cross-IDE skills:** the same OpenSpec skill files are duplicated across `.claude/`, `.codex/`, `.cursor/`, `.kimi/`, and `.opencode/`. If you edit one copy, update all of them.
- **Do not commit generated build output** (`firmware/.pio/`, `android/**/build/`).

## Testing instructions

Automated unit tests exist on both sides; Android instrumentation also exercises the actual Compose renderer on an emulator:

- **Agent CLI** — `firmware/.venv/Scripts/python.exe -m unittest discover -s tools -v` runs the host tests for the protocol, lifecycle, network and UART reset evidence; these are not hardware OTA acceptance.
- **Firmware** — 109 tests in eight Unity suites, including mixed-mode Undo, journal exhaustion, BLE queue saturation, display and OTA resource/transaction fault injection. Run with `cd firmware && .venv/Scripts/pio.exe test -e native`. After building `esp32dev`, `ota` and `ota_b`, run `scripts/check_scoring_build.py` to check linked journal RAM, allocator independence and ATT routing. Never run bare `pio test`; it also waits for hardware.
- **Offline OTA tooling** — `scripts/test_ota_tool.py --output-dir <new-external-session>` runs15 real OpenSSL/package tests against built A/B images, generating disposable TEST ONLY keys outside Git. Browser HTML/JS is tested against a labelled local mock, not real ESP32 HTTP. Do not ship/provision those disposable keys.
- **Android** — 43 JUnit tests: parser18, scoring17, GATT session6, pixels2; plus three Compose emulator tests covering exact portrait pixel mapping, control dispatch, destructive actions and reconnect gating. Run with `cd android && ./gradlew testDebugUnitTest` (requires `JAVA_HOME`, see Build and test commands).
- Logic changes must stay faithful to the authoritative specs; the scoring suite encodes the worked examples in `docs/specs/01-scoring-state-machine.md` Section 7.
- Firmware/Android **builds** verified with `pio run -e esp32dev` / `./gradlew assembleDebug`.
- Hardware behavior is validated **manually** using `docs/commissioning-dual-p10.md`. Fresh owner-confirmed isolation is required for USB flashing, wired recovery and serial identification/reset; no external 5 V at the ESP32. Routine installed wireless OTA leaves approved power and HUB75 connected; see `docs/agent-ota-deployment.md` for separate attestations and write gates. Host/emulator and CI passes are not physical BLE/display acceptance.

## Security considerations

- The repository contains no credentials, secrets, or environment files. Do not commit `.env` files, API keys, certificates, or keystores.
- BLE is intentionally **open** for v1: no pairing/bonding/encryption, a single central connection, no OTA over BLE (Spec 02 §8). Local browser OTA is separately authorized under Spec04 and is not a scoring protocol. This is an accepted trade-off for a single-court local device — do not "fix" it without an approved spec change.
- The Android manifest requests BLE permissions (`BLUETOOTH_SCAN` with `neverForLocation`, `BLUETOOTH_CONNECT`, `ACCESS_FINE_LOCATION`) as required by the platform for BLE scanning; keep them minimal.

## Agent-specific notes

- For an exact owner-authorized OTA transaction, retain the owner's operating attestations for this session. Do not ask again unless an observation contradicts them or circumstances materially change. Still verify device identity, live image and health, BLE occupancy, target slot and memory admission. Routine installed OTA does not require USB or enclosure access.
- For a recoverable pre-write OTA failure, diagnose it and apply the bounded recovery in `docs/agent-ota-deployment.md`. Before any owner-budgeted reactivation, establish closure or expiry, check BLE recovery, and verify host restoration. Preserve the exact scope and one-write limit. Never retry an uncertain upload or install, and never treat this session's authorization as standing permission.
- Before implementing a feature, use the OpenSpec workflow to create or select a change and read all `contextFiles` returned by `openspec instructions apply --change "<name>" --json`.
- Keep implementation changes minimal and scoped to the active task in `tasks.md`.
- If implementation reveals a design problem, pause and update the relevant spec/artifact (`docs/specs/` and/or the OpenSpec change) rather than silently diverging — the specs are the contract between firmware and app.
- If the user asks for something not covered by an existing spec or change, offer to create a proposal first.
- Known explicit non-goals for v1 (do not build unless requested): match-point/win detection, animations, brightness control, multi-phone control, Wi-Fi/internet scoring sync. Local browser OTA is now explicitly requested/implemented under Spec04; no cloud/internet/BLE firmware transfer is included. (SINGLES mode is no longer a non-goal — it was implemented via archived change `2026-07-23-implement-singles-mode`.)
- **Git state:** the singles-mode feature is intentionally **uncommitted** in the working tree — modified `docs/specs/01` and `02`, firmware + Android sources and tests, plus untracked `openspec/changes/archive/2026-07-23-implement-singles-mode/` and `openspec/specs/ble-protocol/`. Do not discard (`git checkout`/`git restore`) or commit it unless explicitly asked. The `firmware/.pio` build artifacts were untracked on 2026-08-06.

## Cross-project UX, DX, and AX principles

Cut taglines, helper text and secondary labels. If a screen needs them to make sense, fix the layout.

Make the best decision that will elevate all three: the user experience (UX), developer experience (DX), and agent experience (AX) always, without breaking anything.
