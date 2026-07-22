## Why

The repository currently contains only OpenSpec planning scaffolding and no application code. Before implementing the scoring state machine (Spec 01), BLE protocol (Spec 02), and display rendering (Spec 03), we need a working build system and source tree for both the ESP32 firmware and the Android app so that subsequent logic changes compile and can be validated incrementally.

## What Changes

- Create `firmware/` as a PlatformIO project targeting the ESP32-WROOM-32, with stub headers/sources for game state, BLE service, and display rendering wired together from `main.cpp`.
- Create `android/` as an Android Studio Gradle project using Kotlin and Jetpack Compose, with a stub BLE client, a 64×32 logical preview canvas, and the six control buttons from Spec 01.
- Create `shared/display_assets/` with two JSON data files: the 5×7 digit font bitmaps from Spec 03 Section 4a and the `ARROW_RIGHT` bitmap from Spec 03 Section 4b (left arrow derived at runtime by mirroring).
- **Explicitly not included**: state transition logic, BLE read/write command handling, pixel rendering, or match-point/win detection. Those belong in follow-up changes.

## Capabilities

### New Capabilities

- `shared-display-assets`: Defines the JSON file format for the shared 5×7 digit font and 9×7 arrow bitmap so both firmware and app source identical glyph data. The approved Spec 03 Section 6 recommends such a shared layout file but does not define its schema; this change adds that definition.

### Modified Capabilities

None. No requirement changes are proposed for the already-approved capabilities in:

- `docs/specs/01-scoring-state-machine.md`
- `docs/specs/02-ble-protocol.md`
- `docs/specs/03-display-rendering.md`

This change only scaffolds their implementation.

## Impact

- New top-level directories: `firmware/`, `android/`, `shared/display_assets/`.
- Adds PlatformIO and Gradle build configuration; no existing code is modified.
- Establishes the file layout that future logic-implementing changes will fill in.
