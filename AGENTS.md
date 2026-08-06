# AGENTS.md

This file is intended for AI coding agents working on the `pickleball_scoreboard` project.

## Project overview

A **pickleball scoreboard** system with two implementations driven by one shared state:

- **ESP32 firmware** (`firmware/`) — drives a physical 64×32 HUB75 LED matrix panel, owns the authoritative game state, and acts as a BLE peripheral.
- **Android app** (`android/`) — a Jetpack Compose remote control and live preview. It is a "dumb renderer": it sends button-press commands over BLE and displays whatever state the ESP32 reports back. It never computes scoring logic.

**Current state:** fully implemented and unit tested. Firmware state transitions, BLE command handling, and pixel rendering are done, with three host-side Unity test suites; the Android app implements BLE scan/connect/write, state parsing, and rendering, with two JUnit suites. There is no CI and no deployment process; remaining work is manual hardware-in-the-loop validation (see Active changes).

### Authoritative specs

Behavior is defined by three **approved** specs in `docs/specs/` — these are the single source of truth, and both implementations must follow them faithfully:

| Spec | Defines |
|---|---|
| `docs/specs/01-scoring-state-machine.md` | Game state fields, six inputs (rally won L/R, undo, switch courts, end game, reset), transition rules, single-step undo, 0-0-2 game start |
| `docs/specs/02-ble-protocol.md` | BLE GATT structure (one service, Write-only command characteristic, Notify-only state characteristic), ASCII command format (single-byte commands; reset is a 3-byte sequence, §4a), comma-separated state payload, device name `"PickleScore"` |
| `docs/specs/03-display-rendering.md` | 64×32 logical canvas (2×2 grid of 32×16 panels), 5×7 digit font, 9×7 arrow bitmap (left derived by mirroring right), white-on-black colors, layout regions |

When implementing anything, cite the spec section in code comments (the existing code already does this, e.g. `// Spec 01 Section 4`).

## Repository layout

```text
.
├── README.md                     # One-line project title
├── AGENTS.md                     # This file
├── docs/
│   └── specs/                    # Authoritative behavior specs 01–03 (APPROVED)
├── firmware/                     # ESP32 firmware (PlatformIO, Arduino framework)
│   ├── platformio.ini            # env:esp32dev (HUB75 + Adafruit GFX lib_deps) + env:native host test env
│   ├── include/                  # game_state.h, ble_command_parser.h, ble_service.h, display_render_logic.h, display_render.h
│   ├── src/                      # main.cpp + module implementations
│   ├── test/                     # Unity suites: test_game_state, test_ble_command_parsing, test_display_render
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
│       ├── layout.json           # Layout constants: canvas, score centers, tops, gaps, divider
│       └── arrows.json           # ARROW_RIGHT only (9×7); left is derived by mirroring rows
├── openspec/                     # OpenSpec planning framework (schema: spec-driven)
│   ├── config.yaml
│   ├── changes/                  # Active changes + archive/
│   └── specs/                    # Main capability specs (shared-display-assets, scoring-state-machine-tests, ble-protocol)
├── .claude/ .codex/ .cursor/ .kimi/ .opencode/   # Duplicated OpenSpec agent tooling
└── firmware/.pio/                # PlatformIO build output (generated, do not edit)
```

## Technology stack

- **Firmware:** C++, PlatformIO Core 6.1.19, `framework = arduino`, board `esp32dev` (ESP32-WROOM-32). Libraries: `mrfaptastic/ESP32 HUB75 LED MATRIX PANEL DMA Display` (with `adafruit/Adafruit GFX Library`) for the LED panel, and the Arduino `BLEDevice` stack for BLE. Serial monitor at 115200 baud.
- **Android:** Kotlin 1.9.25, Android Gradle Plugin 8.5.1, Jetpack Compose (BOM 2024.06.00, compiler extension 1.5.15), `minSdk = 26`, `compileSdk/targetSdk = 35`, JVM target 1.8. Gradle wrapper 8.9.
- **Shared data:** plain JSON glyph + layout files, parsed at runtime by the Android app only (no code generation); the firmware keeps hand-transcribed copies in `display_render_logic.cpp` (see Shared below).
- **Planning:** OpenSpec CLI with the `spec-driven` schema.

## Build and test commands

### Firmware (PlatformIO)

A working PlatformIO install lives in the project's own venv at `firmware/.venv`:

```bash
cd firmware
.venv/Scripts/pio.exe run -e esp32dev    # build (Windows; use .venv/bin/pio on POSIX)
.venv/Scripts/pio.exe run -t upload    # flash to a connected ESP32
.venv/Scripts/pio.exe device monitor   # serial monitor, 115200 baud
.venv/Scripts/pio.exe test -e native   # run Unity host tests (test_game_state, test_ble_command_parsing, test_display_render)
```

(Any globally installed `pio` works too.) The `-e esp32dev` is required for builds: bare `pio run` also builds the `native` env, which is test-only and fails to link outside `pio test`. The firmware is known to compile cleanly with `pio run -e esp32dev`.

**WARN:** always pass `-e native` when running firmware tests. Bare `pio test` processes the `esp32dev` env first and hangs waiting for hardware (documented in `firmware/pio_test_debug.log`). The `native` env needs a `g++` toolchain on PATH — MSYS2 MinGW g++ exists on this host at `/c/msys64/mingw64/bin`.

### Android (Gradle)

```bash
cd android
./gradlew assembleDebug          # build debug APK
./gradlew testDebugUnitTest      # run JUnit unit tests (StatePayloadParserTest, ScoreboardStateMachineTest)
```

**NOTE:** this host has no JDK on PATH and `JAVA_HOME` unset. In Git Bash, point Gradle at the Android Studio bundled JBR first: `export JAVA_HOME="/c/Program Files/Android/Android Studio/jbr"`.

Both test suites are pure host-side unit tests (no emulator, no hardware). There is no CI; firmware hardware behavior is validated manually via the E2E checklists in the active changes.

### OpenSpec (planning)

```bash
openspec new change "<name>"
openspec status --change "<name>" --json
openspec instructions <artifact> --change "<name>" --json
openspec list --json
openspec validate --change "<name>"
openspec archive --change "<name>"
```

## Architecture and module division

### Firmware (`firmware/`)

Five modules wired together in `src/main.cpp`. The three hardware-free modules (`game_state`, `ble_command_parser`, `display_render_logic`) are the ones covered by the `native` test env's `build_src_filter` in `platformio.ini`:

- `game_state.h/.cpp` — `GameState` struct (`leftScore`, `rightScore`, `servingSide`, `serverNumber`, `gameMode`, `gameEnded`) and one handler per Spec 01 input; fully implemented. Undo is single-step, implemented by saving a copy of the whole state before each transition (Spec 01 §5a). `handleReset` takes `(GameState&, Side startingSide, GameMode mode = GameMode::DOUBLES)` (Spec 01 §9, Spec 02 §4a).
- `ble_command_parser.h/.cpp` — pure parser `handleBleCommand(const std::string& value, GameState& state)`: maps the Spec 02 §4 ASCII command bytes (including the 3-byte reset sequence) to the `game_state` handlers. No I/O, unit tested on the host.
- `ble_service.h/.cpp` — GATT server setup per Spec 02 §3: service `4fafc201-1fb5-459e-8fcc-c5c9c331914b`, command characteristic `...26a8` (Write), state characteristic `...26a9` (Notify), advertised as `"PickleScore"`. Wires the command Write callback to `handleBleCommand` and notifies the serialized state payload.
- `display_render_logic.h/.cpp` — pure pixel computation `computeRenderedPixels(const GameState& state)` returning the lit pixels for the 64×32 canvas. Deliberately embeds hand-transcribed glyph arrays and layout constants instead of parsing the shared JSON at runtime — an approved deviation from Spec 03 §6 documented in the header comment of `src/display_render_logic.cpp`.
- `display_render.h/.cpp` — owns the global `MatrixPanel_I2S_DMA dma_display`, configured for a 2×2 chain of 32×16 panels = 64×32 canvas (`DISPLAY_WIDTH`/`DISPLAY_HEIGHT` constants); thin adapter that draws the pixels from `display_render_logic`.

### Android (`android/`)

Package `com.example.pickleballscoreboard`:

- `MainActivity.kt` — hosts `ScoreboardScreen()` and wires its button handlers to `BleClient`.
- `ble/BleClient.kt` — fully implemented: scan/connect/subscribe/write. Holds the Spec 02 UUIDs and command byte constants (`'L'`, `'R'`, `'U'`, `'C'`, `'E'`, `'0'`; reset is the **three-byte** sequence `'0'` + `'L'|'R'` + `'D'|'S'`, Spec 02 §4a amended for SINGLES). Deliberate: `CMD_RESET_SIDE_LEFT`/`CMD_RESET_SIDE_RIGHT` intentionally share byte values with `CMD_RALLY_WON_LEFT`/`CMD_RALLY_WON_RIGHT` but are kept as distinct constants (comment in the file explains why).
- `state/ScoreboardState.kt` + `state/ScoreboardStateMachine.kt` — state model (`Side`, `GameMode`, `ScoreboardState`) and a Kotlin mirror of the Spec 01 state machine (including SINGLES), covered by `ScoreboardStateMachineTest`. The BLE Notify payload parser `parseStatePayload` (Spec 02 §5) lives in `ble/BleClient.kt` and is covered by `StatePayloadParserTest`.
- `ui/ScoreboardScreen.kt` — Compose UI: a `Canvas` with the 64×32 logical grid (2:1 aspect, scaled to screen width) and the six Spec 01 control buttons, wired to real handlers via `MainActivity`.
- `ui/DisplayRenderer.kt` — draws the reported state onto the canvas using the shared glyph + layout JSON (Spec 03).

Assets wiring: `app/build.gradle.kts` adds `../../shared/display_assets` as an assets `srcDir` so the glyph and layout JSON ship in the APK (Spec 03 §6).

### Shared (`shared/display_assets/`)

- `font_5x7.json` — digits `0`–`9`, each an array of 7 strings of 5 chars, `1` = lit.
- `arrows.json` — `ARROW_RIGHT` only (7 strings of 9 chars). `ARROW_LEFT` must be derived at runtime by reversing each row (Spec 03 §4b); never author a second bitmap.
- `layout.json` — shared layout constants (Spec 03 §2/§4): 64×32 canvas, left/right score centers, arrow/digit tops, 1px gaps, divider x.

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

(Per `openspec list --json`:)

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

Automated unit tests exist on both sides (all host-side; no emulator or hardware needed):

- **Firmware** — three Unity suites under `firmware/test/`: `test_game_state` (17 tests), `test_ble_command_parsing` (22), `test_display_render` (6). Run with `cd firmware && .venv/Scripts/pio.exe test -e native`. The `native` env builds only the three hardware-free modules via `build_src_filter`. WARN: bare `pio test` tries `esp32dev` first and hangs waiting for hardware (`firmware/pio_test_debug.log`); a `g++` toolchain must be on PATH.
- **Android** — 35 JUnit tests under `android/app/src/test/`: `StatePayloadParserTest` (18) and `ScoreboardStateMachineTest` (17). Run with `cd android && ./gradlew testDebugUnitTest` (requires `JAVA_HOME`, see Build and test commands).
- Logic changes must stay faithful to the authoritative specs; the scoring suite encodes the worked examples in `docs/specs/01-scoring-state-machine.md` Section 7.
- Firmware/Android **builds** still verified with `pio run` / `./gradlew assembleDebug`.
- Hardware behavior is validated **manually**: each active change carries a `manual-e2e-checklist.md` (`implement-ble-wiring`, `implement-display-rendering`). There is no CI.

## Security considerations

- The repository contains no credentials, secrets, or environment files. Do not commit `.env` files, API keys, certificates, or keystores.
- BLE is intentionally **open** for v1: no pairing/bonding/encryption, a single central connection, no OTA updates (Spec 02 §8). This is an accepted trade-off for a single-court local device — do not "fix" it without an approved spec change.
- The Android manifest requests BLE permissions (`BLUETOOTH_SCAN` with `neverForLocation`, `BLUETOOTH_CONNECT`, `ACCESS_FINE_LOCATION`) as required by the platform for BLE scanning; keep them minimal.

## Agent-specific notes

- Before implementing a feature, use the OpenSpec workflow to create or select a change and read all `contextFiles` returned by `openspec instructions apply --change "<name>" --json`.
- Keep implementation changes minimal and scoped to the active task in `tasks.md`.
- If implementation reveals a design problem, pause and update the relevant spec/artifact (`docs/specs/` and/or the OpenSpec change) rather than silently diverging — the specs are the contract between firmware and app.
- If the user asks for something not covered by an existing spec or change, offer to create a proposal first.
- Known explicit non-goals for v1 (do not build unless requested): match-point/win detection, animations, brightness control, multi-phone control, Wi-Fi/internet sync, OTA updates. (SINGLES mode is no longer a non-goal — it was implemented via archived change `2026-07-23-implement-singles-mode`.)
- **Git state:** the singles-mode feature is intentionally **uncommitted** in the working tree — modified `docs/specs/01` and `02`, firmware + Android sources and tests, plus untracked `openspec/changes/archive/2026-07-23-implement-singles-mode/` and `openspec/specs/ble-protocol/`. Do not discard (`git checkout`/`git restore`) or commit it unless explicitly asked. The `firmware/.pio` build artifacts were untracked on 2026-08-06.
