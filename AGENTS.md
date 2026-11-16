# AGENTS.md

This file is intended for AI coding agents working on the `pickleball_scoreboard` project.

## Project overview

A **pickleball scoreboard** system with two implementations driven by one shared state:

- **ESP32 firmware** (`firmware/`) — drives two horizontal 32×16 P10 HUB75 panels (64×16), owns the authoritative game state, and acts as a BLE peripheral. Brightness is fixed at 12/255.
- **Android app** (`android/`) — a Jetpack Compose remote control and live preview. It is a "dumb renderer": it sends button-press commands over BLE and displays whatever state the ESP32 reports back. It never computes scoring logic.

**Current state:** production 64×16 migration implemented; 51 firmware Unity tests, 42 Android JUnit tests and one emulator Compose/pixel test pass. Actual C++ and Kotlin renderers match 48 complete shared pixel snapshots. Production firmware was uploaded after fresh owner isolation confirmation; USB-only serial boot passed (64×16, chain2, brightness12,0-0-2, BLE initialization,135592 bytes free heap). Physical scoreboard/BLE E2E remains owner-unverified; see `docs/reports/dual-p10-flash-verification.md`. See `docs/commissioning-dual-p10.md`; diagnostic bring-up is owner-accepted and preserved, not the production app.

### Authoritative specs

Behavior is defined by **approved** specs in `docs/specs/` — these are the single source of truth, and both implementations must follow them faithfully:

| Spec | Defines |
|---|---|
| `docs/specs/01-scoring-state-machine.md` | Game state fields, six inputs (rally won L/R, undo, switch courts, end game, reset), transition rules, single-step undo, 0-0-2 game start |
| `docs/specs/02-ble-protocol.md` | BLE GATT structure (one service, Write-only command characteristic, Notify-only state characteristic), ASCII command format (single-byte commands; reset is a 3-byte sequence, §4a), comma-separated state payload, device name `"PickleScore"` |
| `docs/specs/03-display-rendering.md` | 64×16 logical canvas (two horizontal 32×16 panels), 5×7 digits scaled 2×, 5×5 arrows (left mirrored), white-on-black, separate arrow gutters, black seam |
| `docs/specs/04-local-browser-ota.md` | Opt-in SoftAP/browser maintenance, private provisioning, signed inactive-slot OTA, BLE priority, rollback and explicit initial-migration authorization |

**OTA feature:** `feat/browser-softap-ota` preserves the working `feat/dual-p10-scoreboard` baseline. Profiles `ota`/`ota_b` build signed-package-capable production firmware with two0x1f0000 slots; owner-approved initial USB migration/provisioning is installed as `ota-v1-a` in `ota_0` (pending0/UNDEFINED). Full4MB backup, exact private trust record readback and all four written regions are verified; no bootloader/eFuse changes. AP-start heap~59.8KB, post-idle closure~104–107KB. Repeated SDK `wifi:timeout when WiFi un-init, type=4` remains unresolved; do not call diagnostics entirely clean or seal on startup evidence. See `docs/reports/browser-softap-ota-installation.md`. See `docs/ota-maintenance.md` and `docs/reports/browser-softap-ota-verification.md`. Never infer fresh electrical isolation or partition-migration approval from the earlier production upload. No default credentials/private signing trust; generate owner files outside Git. The current owner confirms baseline initial display/both panels work; OTA/RF/heap/rollback execution still needs commissioning.

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
│       ├── layout.json           # 64×16 canvas, scaled score positions and outer-arrow gutters
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
.venv/Scripts/pio.exe test -e native   # five hardware-free Unity suites (69 tests)
.venv/Scripts/pio.exe run -e ota -e ota_b # build only; NEVER generic first-migration upload
```

(Any globally installed `pio` works too.) The `-e esp32dev` is required for builds: bare `pio run` also builds the `native` env, which is test-only and fails to link outside `pio test`. The firmware is known to compile cleanly with `pio run -e esp32dev`.

**WARN:** always pass `-e native` when running firmware tests. Bare `pio test` processes the `esp32dev` env first and hangs waiting for hardware (documented in `firmware/pio_test_debug.log`). The `native` env needs a `g++` toolchain on PATH — MSYS2 MinGW g++ exists on this host at `/c/msys64/mingw64/bin`.

On this Windows host, native MinGW has an unrelated `libisl-23.dll` failure. Do not repair it for this task: run `firmware/.venv/Scripts/python.exe scripts/run_firmware_host_tests.py --output-dir <external-session-artifacts>/host` from the repository root using installed MSVC. See commissioning docs for cross-language snapshots.

### Android (Gradle)

```bash
cd android
./gradlew assembleDebug          # build debug APK
./gradlew testDebugUnitTest      # 42 pure host JUnit tests
./gradlew connectedDebugAndroidTest # actual Compose pixels/controls on a running emulator
```

**NOTE:** this host has no JDK on PATH and `JAVA_HOME` unset. In Git Bash, point Gradle at the Android Studio bundled JBR first: `export JAVA_HOME="/c/Program Files/Android/Android Studio/jbr"`.

Unit tests need no emulator or hardware; the additional Compose instrumentation test needs an emulator. This host's global `~/.gradle/init.gradle` forces obsolete Flutter test libraries: use an isolated session `GRADLE_USER_HOME` for instrumentation, leaving that global file untouched. ATD screenshots require drawing enabled; successful setup and evidence are documented in commissioning docs. There is no CI; real panel/BLE acceptance remains manual.

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

- `game_state.h/.cpp` — `GameState` struct (`leftScore`, `rightScore`, `servingSide`, `serverNumber`, `gameMode`, `gameEnded`) and one handler per Spec 01 input; fully implemented. Undo is single-step, implemented by saving a copy of the whole state before each transition (Spec 01 §5a). `handleReset` takes `(GameState&, Side startingSide, GameMode mode = GameMode::DOUBLES)` (Spec 01 §9, Spec 02 §4a).
- `ble_command_parser.h/.cpp` — pure parser `handleBleCommand(const std::string& value, GameState& state)`: maps the Spec 02 §4 ASCII command bytes (including the 3-byte reset sequence) to the `game_state` handlers. No I/O, unit tested on the host.
- `ble_service.h/.cpp` — GATT server setup per Spec 02 §3: service `4fafc201-1fb5-459e-8fcc-c5c9c331914b`, command characteristic `...26a8` (Write), state characteristic `...26a9` (Notify), advertised as `"PickleScore"`. Callbacks enqueue bounded command records; only the Arduino loop runs `handleBleCommand`. CCCD enable requests initial sync; connect alone never notifies. `ble_sync_gate.h` and `state_payload.h` are host-tested.
- `display_render_logic.h/.cpp` — pure pixel computation `computeRenderedPixels(const GameState& state)` returning the lit pixels for the 64×16 canvas. Deliberately embeds hand-transcribed glyph arrays and layout constants instead of parsing the shared JSON at runtime — an approved deviation from Spec 03 §6 documented in the header comment of `src/display_render_logic.cpp`.
- `display_render.h/.cpp` — guarded DMA initialization, explicit accepted pins/SHIFTREG/TYPE138/8MHz/clock phase, two 32×16 panels, brightness 12. `p10_hardware.h` swaps chain blocks while preserving local coordinates. Draws only on state changes; no virtual/four-scan remapping.

- `ota_maintenance.h/.cpp` — opt-in local AP/HTTP/provisioning and deferred boot-confirmation adapter. `ota_core.h` contains the same policy/auth/request/header/stream transaction exercised by18 host tests; `ota_credentials.h` fixes the record format and `ota_web.h` contains offline HTML/JS. `partitions_ota.csv` is an explicit owner-gated migration, not a drop-in ordinary upload. `scripts/ota_tool.py` handles private provisioning/signing; `scripts/check_ota_build.py` inspects/packages public builds without serial access.

### Android (`android/`)

Package `com.example.pickleballscoreboard`:

- `MainActivity.kt` — hosts `ScoreboardScreen()` and wires its button handlers to `BleClient`.
- `ble/BleClient.kt` — fully implemented: scan/connect/subscribe/write. Holds the Spec 02 UUIDs and command byte constants (`'L'`, `'R'`, `'U'`, `'C'`, `'E'`, `'0'`; reset is the **three-byte** sequence `'0'` + `'L'|'R'` + `'D'|'S'`, Spec 02 §4a amended for SINGLES). Deliberate: `CMD_RESET_SIDE_LEFT`/`CMD_RESET_SIDE_RIGHT` intentionally share byte values with `CMD_RALLY_WON_LEFT`/`CMD_RALLY_WON_RIGHT` but are kept as distinct constants (comment in the file explains why).
- `state/ScoreboardState.kt` + `state/ScoreboardStateMachine.kt` — state model (`Side`, `GameMode`, `ScoreboardState`) and a Kotlin mirror of the Spec 01 state machine (including SINGLES), covered by `ScoreboardStateMachineTest`. The strict six-field Notify parser `parseStatePayload` lives in `ble/StatePayloadParser.kt`. `ble/GattSession.kt` gates readiness on subscription plus initial valid state and serializes acknowledged command writes; both are covered by JUnit.
- `ui/ScoreboardScreen.kt` — Compose 64×16/4:1 preview and six Spec 01 controls, wired through `MainActivity` only after BLE synchronization.
- `ui/DisplayRenderer.kt` — draws the reported state using pure `ScoreboardPixels.kt`, shared JSON and the exact pixel sets exercised by JUnit and actual emulator capture tests.

Assets wiring: `app/build.gradle.kts` adds `../../shared/display_assets` as an assets `srcDir` so the glyph and layout JSON ship in the APK (Spec 03 §6).

### Shared (`shared/display_assets/`)

- `font_5x7.json` — digits `0`–`9`, each an array of 7 strings of 5 chars, `1` = lit.
- `arrows.json` — `ARROW_RIGHT` only (5 strings of 5 chars); derive left by reversing each row, never author a second bitmap.
- `layout.json` — 64×16 canvas, 2× digit scale, 2px digit gaps, score centers18.5/44.5, outer arrow x1/58. No divider; seam x30..33 and outer boundary remain black.
- `../render_fixtures/` — 48 complete versioned pixel snapshots and CSV inputs. `scripts/check_display_snapshots.py` compares the independent JSON oracle with actual C++ output; Android JUnit consumes the same snapshots.

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

Automated unit tests exist on both sides; one additional Android instrumentation test exercises the real Compose renderer on an emulator:

- **Firmware** — 69 tests in five Unity suites, including `test_ota_core` (18 policy/auth/header/fault-injection tests) plus the original51: `test_game_state` (17), `test_ble_command_parsing` (22), `test_display_render` (6), `test_protocol_lifecycle` (6). Run with `cd firmware && .venv/Scripts/pio.exe test -e native`. The `native` env builds only the three hardware-free modules via `build_src_filter`. WARN: bare `pio test` tries `esp32dev` first and hangs waiting for hardware (`firmware/pio_test_debug.log`); a `g++` toolchain must be on PATH.
- **Offline OTA tooling** — `scripts/test_ota_tool.py --output-dir <new-external-session>` runs15 real OpenSSL/package tests against built A/B images, generating disposable TEST ONLY keys outside Git. Browser HTML/JS is tested against a labelled local mock, not real ESP32 HTTP. Do not ship/provision those disposable keys.
- **Android** — 42 JUnit tests: parser18, scoring17, GATT session5, pixels2; plus one actual Compose emulator test checking all 1024 logical pixel centers for 0-0, 10-25 and 99-99, 4:1 bounds and control callbacks. Run with `cd android && ./gradlew testDebugUnitTest` (requires `JAVA_HOME`, see Build and test commands).
- Logic changes must stay faithful to the authoritative specs; the scoring suite encodes the worked examples in `docs/specs/01-scoring-state-machine.md` Section 7.
- Firmware/Android **builds** verified with `pio run -e esp32dev` / `./gradlew assembleDebug`.
- Hardware behavior is validated **manually** using `docs/commissioning-dual-p10.md`. Fresh owner-confirmed isolated USB-only setup is required before any upload or serial-resetting identification; no external 5V at the ESP32. Host/emulator passes are not physical BLE/display acceptance. There is no CI.

## Security considerations

- The repository contains no credentials, secrets, or environment files. Do not commit `.env` files, API keys, certificates, or keystores.
- BLE is intentionally **open** for v1: no pairing/bonding/encryption, a single central connection, no OTA over BLE (Spec 02 §8). Local browser OTA is separately authorized under Spec04 and is not a scoring protocol. This is an accepted trade-off for a single-court local device — do not "fix" it without an approved spec change.
- The Android manifest requests BLE permissions (`BLUETOOTH_SCAN` with `neverForLocation`, `BLUETOOTH_CONNECT`, `ACCESS_FINE_LOCATION`) as required by the platform for BLE scanning; keep them minimal.

## Agent-specific notes

- Before implementing a feature, use the OpenSpec workflow to create or select a change and read all `contextFiles` returned by `openspec instructions apply --change "<name>" --json`.
- Keep implementation changes minimal and scoped to the active task in `tasks.md`.
- If implementation reveals a design problem, pause and update the relevant spec/artifact (`docs/specs/` and/or the OpenSpec change) rather than silently diverging — the specs are the contract between firmware and app.
- If the user asks for something not covered by an existing spec or change, offer to create a proposal first.
- Known explicit non-goals for v1 (do not build unless requested): match-point/win detection, animations, brightness control, multi-phone control, Wi-Fi/internet scoring sync. Local browser OTA is now explicitly requested/implemented under Spec04; no cloud/internet/BLE firmware transfer is included. (SINGLES mode is no longer a non-goal — it was implemented via archived change `2026-07-23-implement-singles-mode`.)
- **Git state:** the singles-mode feature is intentionally **uncommitted** in the working tree — modified `docs/specs/01` and `02`, firmware + Android sources and tests, plus untracked `openspec/changes/archive/2026-07-23-implement-singles-mode/` and `openspec/specs/ble-protocol/`. Do not discard (`git checkout`/`git restore`) or commit it unless explicitly asked. The `firmware/.pio` build artifacts were untracked on 2026-08-06.

## Cross-project UX, DX, and AX principles

Cut taglines, helper text and secondary labels. If a screen needs them to make sense, fix the layout.

Make the best decision that will elevate all three: the user experience (UX), developer experience (DX), and agent experience (AX) always, without breaking anything.
