## Why

The firmware and Android app currently have only empty `game_state` stubs. Before wiring BLE commands or display rendering, both platforms need the exact same scoring state machine implemented so that every input produces the same state on the ESP32 and in the app preview.

## What Changes

- Fill in `firmware/src/game_state.cpp` with the transition logic from Spec 01 Sections 4–5c (initial state, rally won left/right, switch courts, single-step undo, end game, reset).
- Add a Kotlin state holder (`ScoreboardState` / `ScoreboardViewModel`) in the Android app with logically identical semantics.
- Add unit tests on both platforms covering Spec 01 Section 7 plus one test per transition type (Case A, Case B1, Case B2, switch courts, undo, end-game freeze, reset).
- **Explicitly not included**: BLE command parsing/notifications (Spec 02), pixel rendering (Spec 03), SINGLES mode, match-point/win detection.

## Capabilities

### New Capabilities

- `scoring-state-machine-tests`: Defines the unit-test coverage required to prove that the firmware C++ and Android Kotlin state-machine implementations both satisfy `docs/specs/01-scoring-state-machine.md`.

### Modified Capabilities

None. This change implements the already-approved requirements in `docs/specs/01-scoring-state-machine.md` without changing the spec itself. The existing `shared-display-assets` capability is not affected.

## Impact

- Touches `firmware/src/game_state.cpp` and adds firmware tests.
- Adds new Kotlin source(s) under `android/app/src/main/java/.../` for the state holder and tests under `android/app/src/test/.../`.
- No changes to BLE stubs, display stubs, JSON assets, or build configuration beyond adding test dependencies if required.
