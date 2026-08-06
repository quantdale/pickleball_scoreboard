## Why

Spec 01 Section 9a and Spec 02 Section 4a were just updated to fully define SINGLES mode: a single fault causes immediate side-out (no B1/B2 split), and RESET becomes a 3-byte command that also carries game mode. The state machine and BLE parser on both firmware and Android still only implement the DOUBLES-only, 2-byte-reset behavior from before these spec updates. This change brings both platforms into compliance with the current specs.

## What Changes

- **BREAKING**: RESET command changes from 2 bytes (`'0'` + side) to 3 bytes (`'0'` + side + mode) per Spec 02 Section 4a. A 2-byte or otherwise incomplete reset is now malformed input and is ignored — there is no fallback to the old format.
- Firmware `game_state.cpp`/`game_state.h`: `handleReset` takes a `GameMode` parameter; the rally-won Case B logic branches on `gameMode` — DOUBLES keeps the existing B1/B2 split, SINGLES does immediate side-out with no B1 step, per Spec 01 Section 9a.
- Firmware `ble_command_parser.cpp`: parses the 3-byte reset command; a 2-byte reset (or any other malformed reset) is ignored per Spec 02 Section 7.
- Firmware `ble_service.cpp`: `notifyState()` payload gains a `gameMode` field (`'D'`/`'S'`) per Spec 02 Section 5's own forward-looking note ("Add it here when singles support is actually built") — see design.md for the resolved decision.
- Android `ScoreboardStateMachine`/`ScoreboardState`: mirrors the same SINGLES branching in `reset`/`applyRally`.
- Android `BleClient`: reset command writer sends 3 bytes; state payload parser reads the new `gameMode` field.
- Android `MainActivity`: reset dialog adds a Singles/Doubles selector alongside the existing side selector.
- Tests on both platforms: new coverage for SINGLES Case B (immediate side-out), 3-byte reset parsing (valid SINGLES, valid DOUBLES, malformed 2-byte rejected), and updated notify-payload parsing with the new `gameMode` field. No test may assert the old 2-byte reset format still works.

## Capabilities

### New Capabilities

- `ble-protocol`: not yet present in `openspec/specs/` (it was proposed by the still-open, unarchived `implement-ble-wiring` change, so there is no base to modify against). This change's delta spec captures the protocol's requirements as they stand today, including the 3-byte reset and `gameMode` notify field — reconcile with `implement-ble-wiring`'s own delta whenever both changes are synced/archived.

### Modified Capabilities

- `scoring-state-machine-tests`: extend firmware test coverage to include SINGLES mode Case B (immediate side-out, no second-server step), per Spec 01 Section 9a.

## Impact

- `firmware/include/game_state.h`, `firmware/src/game_state.cpp` — `handleReset` signature change, Case B branch on `gameMode`.
- `firmware/include/ble_command_parser.h`, `firmware/src/ble_command_parser.cpp` — 3-byte reset parsing, breaking change to malformed-input handling.
- `firmware/src/ble_service.cpp` — `notifyState()` payload format gains a field.
- `firmware/test/test_game_state/test_game_state.cpp`, `firmware/test/test_ble_command_parsing/test_ble_command_parsing.cpp` — new/updated test cases.
- `android/app/src/main/java/com/example/pickleballscoreboard/state/ScoreboardStateMachine.kt` — Case B branch on `gameMode`.
- `android/app/src/main/java/com/example/pickleballscoreboard/ble/BleClient.kt` — reset writer becomes 3 bytes; `parseStatePayload` reads `gameMode`.
- `android/app/src/main/java/com/example/pickleballscoreboard/MainActivity.kt` — reset dialog gains a game-mode selector.
- `android/app/src/test/java/com/example/pickleballscoreboard/state/ScoreboardStateMachineTest.kt`, `android/app/src/test/java/com/example/pickleballscoreboard/ble/StatePayloadParserTest.kt` — new/updated test cases.
