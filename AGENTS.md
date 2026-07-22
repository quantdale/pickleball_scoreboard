# AGENTS.md

This file is intended for AI coding agents working on the `pickleball_scoreboard` project.

## Project overview

A **pickleball scoreboard** system with two implementations driven by one shared state:

- **ESP32 firmware** (`firmware/`) — drives a physical 64×32 HUB75 LED matrix panel, owns the authoritative game state, and acts as a BLE peripheral.
- **Android app** (`android/`) — a Jetpack Compose remote control and live preview. It is a "dumb renderer": it sends button-press commands over BLE and displays whatever state the ESP32 reports back. It never computes scoring logic.

**Current state:** scaffolded only. Both targets compile/build as stubs; state-transition logic, BLE command handling, and pixel rendering are all `TODO` and belong to follow-up changes. There are no tests, no CI, and no deployment process yet.

### Authoritative specs

Behavior is defined by three **approved** specs in `docs/specs/` — these are the single source of truth, and both implementations must follow them faithfully:

| Spec | Defines |
|---|---|
| `docs/specs/01-scoring-state-machine.md` | Game state fields, six inputs (rally won L/R, undo, switch courts, end game, reset), transition rules, single-step undo, 0-0-2 game start |
| `docs/specs/02-ble-protocol.md` | BLE GATT structure (one service, Write-only command characteristic, Notify-only state characteristic), single-ASCII-byte command format, comma-separated state payload, device name `"PickleScore"` |
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
│   ├── platformio.ini            # env:esp32dev, HUB75 + Adafruit GFX lib_deps
│   ├── include/                  # game_state.h, ble_service.h, display_render.h
│   ├── src/                      # main.cpp + stub implementations
│   └── .venv/                    # Local Python venv containing PlatformIO (pio)
├── android/                      # Android app (Gradle, Kotlin, Jetpack Compose)
│   ├── settings.gradle.kts       # rootProject "PickleballScoreboard", :app
│   ├── build.gradle.kts          # Top-level (config lives in app module)
│   ├── gradle.properties
│   ├── gradlew / gradlew.bat     # Gradle 8.9 wrapper
│   └── app/
│       ├── build.gradle.kts      # AGP 8.5.1, Kotlin 1.9.25, Compose BOM 2024.06.00
│       └── src/main/
│           ├── AndroidManifest.xml   # BLE permissions, label "PickleScore"
│           └── java/com/example/pickleballscoreboard/
│               ├── MainActivity.kt
│               ├── ble/BleClient.kt      # BLE client stub (UUIDs + command bytes)
│               └── ui/ScoreboardScreen.kt # Compose screen: 64×32 canvas + 6 buttons
├── shared/
│   └── display_assets/           # Glyph data shared by firmware and app (Spec 03 §6)
│       ├── font_5x7.json         # Digits 0–9 as 5×7 bit patterns
│       └── arrows.json           # ARROW_RIGHT only (9×7); left is derived by mirroring rows
├── openspec/                     # OpenSpec planning framework (schema: spec-driven)
│   ├── config.yaml
│   ├── changes/                  # Active changes + archive/
│   └── specs/                    # Main capability specs (currently empty)
├── .claude/ .codex/ .cursor/ .kimi/ .opencode/   # Duplicated OpenSpec agent tooling
└── firmware/.pio/                # PlatformIO build output (generated, do not edit)
```

## Technology stack

- **Firmware:** C++, PlatformIO Core 6.1.19, `framework = arduino`, board `esp32dev` (ESP32-WROOM-32). Libraries: `mrfaptastic/ESP32 HUB75 LED MATRIX PANEL DMA Display` (with `adafruit/Adafruit GFX Library`) for the LED panel, and the Arduino `BLEDevice` stack for BLE. Serial monitor at 115200 baud.
- **Android:** Kotlin 1.9.25, Android Gradle Plugin 8.5.1, Jetpack Compose (BOM 2024.06.00, compiler extension 1.5.15), `minSdk = 26`, `compileSdk/targetSdk = 35`, JVM target 1.8. Gradle wrapper 8.9.
- **Shared data:** plain JSON glyph files consumed by both sides (no code generation).
- **Planning:** OpenSpec CLI with the `spec-driven` schema.

## Build and test commands

### Firmware (PlatformIO)

A working PlatformIO install lives in the project's own venv at `firmware/.venv`:

```bash
cd firmware
.venv/Scripts/pio.exe run        # build (Windows; use .venv/bin/pio on POSIX)
.venv/Scripts/pio.exe run -t upload    # flash to a connected ESP32
.venv/Scripts/pio.exe device monitor   # serial monitor, 115200 baud
```

(Any globally installed `pio` works too.) The scaffold is known to compile cleanly with `pio run`.

### Android (Gradle)

```bash
cd android
./gradlew assembleDebug          # build debug APK
```

There is **no test suite** on either side yet (no unit tests, no instrumented tests, no CI). "Verification" currently means a clean compile/build plus hardware-in-the-loop testing against the specs.

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

Three modules wired together in `src/main.cpp`, each mirroring one spec:

- `game_state.h/.cpp` — `GameState` struct (`leftScore`, `rightScore`, `servingSide`, `serverNumber`, `gameMode`, `gameEnded`) and one handler per Spec 01 input. All bodies are stubs. Note: undo is single-step and must be implemented by saving a copy of the whole state before each transition (Spec 01 §5a); `handleReset` takes the starting 0-0-2 side parameter (Spec 02 §4a).
- `ble_service.h/.cpp` — GATT server setup (done) per Spec 02 §3: service `4fafc201-1fb5-459e-8fcc-c5c9c331914b`, command characteristic `...26a8` (Write), state characteristic `...26a9` (Notify), advertised as `"PickleScore"`. Command parsing and state serialization are stubs.
- `display_render.h/.cpp` — owns the global `MatrixPanel_I2S_DMA dma_display`, configured for a 2×2 chain of 32×16 panels = 64×32 canvas (`DISPLAY_WIDTH`/`DISPLAY_HEIGHT` constants). Drawing is a stub.

### Android (`android/`)

Package `com.example.pickleballscoreboard`:

- `MainActivity.kt` — hosts `ScoreboardScreen()`.
- `ble/BleClient.kt` — stub holding the Spec 02 UUIDs and command byte constants (`'L'`, `'R'`, `'U'`, `'C'`, `'E'`, `'0'`; reset is the two-byte sequence `'0'` + `'L'|'R'`). Scan/connect/subscribe/write are TODO.
- `ui/ScoreboardScreen.kt` — Compose UI: a `Canvas` with the 64×32 logical grid (2:1 aspect, scaled to screen width) and the six Spec 01 control buttons, all currently wired to no-ops.

### Shared (`shared/display_assets/`)

- `font_5x7.json` — digits `0`–`9`, each an array of 7 strings of 5 chars, `1` = lit.
- `arrows.json` — `ARROW_RIGHT` only (7 strings of 9 chars). `ARROW_LEFT` must be derived at runtime by reversing each row (Spec 03 §4b); never author a second bitmap.

Both firmware and app renderers should source glyph data from these files rather than hard-coding duplicates.

## OpenSpec workflow conventions

This repo follows the OpenSpec `spec-driven` schema defined in `openspec/config.yaml`:

- A **change** is created with `openspec new change "<kebab-case-name>"` and lives under `openspec/changes/<change-name>/` with `proposal.md`, `design.md`, `tasks.md`, and optional delta specs under `specs/`.
- Main capability specs live at `openspec/specs/<capability>/spec.md` (currently empty — the authoritative specs for now are the `docs/specs/` files).
- Completed changes are archived under `openspec/changes/archive/YYYY-MM-DD-<change-name>/`.

### Rules for OpenSpec artifacts

- Always read `openspec status --change "<name>" --json` and `openspec instructions <artifact> --change "<name>" --json` before creating or editing artifacts.
- Use the template and instructions returned by the CLI; the `context` and `rules` fields are constraints for you — do **not** copy them into the artifact.
- Read dependency artifacts before producing a new one.
- Task checkboxes in `tasks.md` use `- [ ]` / `- [x]`; mark tasks complete immediately after finishing them.
- Change names are `kebab-case`.

### Active changes

- `openspec/changes/scaffold-firmware-and-app/` — original scaffolding change (all tasks unchecked; superseded in practice by the `-2` duplicate below).
- `openspec/changes/scaffold-firmware-and-app-2/` — the scaffolding change that was actually executed. All tasks are done **except** task 4.2 (verifying `./gradlew assembleDebug`), which remains unchecked. Neither change has been archived yet.

Available agent workflows (slash commands / skills, duplicated per IDE): `propose`, `apply`, `update`, `sync`, `archive`, `explore`.

## Code style guidelines

- **Match the existing style of the file you're editing.** Firmware uses 4-space indent, Allman-ish braces for functions, `#ifndef` header guards, and spec-section citations in comments. Android code is standard Kotlin with `kotlin.code.style=official`.
- **Comment convention:** reference the governing spec and section (e.g. `// Spec 02 Section 4a`) wherever behavior is spec-defined. Keep comments factual and sparse, like the existing code.
- **Specs are authoritative over code and over the old HTML mockup** (Spec 03 Note A: never port the mockup's JS game logic; Spec 01 rules scoring).
- **Cross-IDE skills:** the same OpenSpec skill files are duplicated across `.claude/`, `.codex/`, `.cursor/`, `.kimi/`, and `.opencode/`. If you edit one copy, update all of them.
- **Do not commit generated build output** (`firmware/.pio/`, `android/**/build/`).

## Testing instructions

No automated test infrastructure exists yet. For now:

- Firmware changes: verify `pio run` compiles; behavior is validated against the worked examples in `docs/specs/01-scoring-state-machine.md` Section 7.
- Android changes: verify `./gradlew assembleDebug` builds.
- Logic changes should be validated against the authoritative specs; when a test runner is introduced, document the command here.

## Security considerations

- The repository contains no credentials, secrets, or environment files. Do not commit `.env` files, API keys, certificates, or keystores.
- BLE is intentionally **open** for v1: no pairing/bonding/encryption, a single central connection, no OTA updates (Spec 02 §8). This is an accepted trade-off for a single-court local device — do not "fix" it without an approved spec change.
- The Android manifest requests BLE permissions (`BLUETOOTH_SCAN` with `neverForLocation`, `BLUETOOTH_CONNECT`, `ACCESS_FINE_LOCATION`) as required by the platform for BLE scanning; keep them minimal.

## Agent-specific notes

- Before implementing a feature, use the OpenSpec workflow to create or select a change and read all `contextFiles` returned by `openspec instructions apply --change "<name>" --json`.
- Keep implementation changes minimal and scoped to the active task in `tasks.md`.
- If implementation reveals a design problem, pause and update the relevant spec/artifact (`docs/specs/` and/or the OpenSpec change) rather than silently diverging — the specs are the contract between firmware and app.
- If the user asks for something not covered by an existing spec or change, offer to create a proposal first.
- Known explicit non-goals for v1 (do not build unless requested): SINGLES mode, match-point/win detection, animations, brightness control, multi-phone control, Wi-Fi/internet sync, OTA updates.
