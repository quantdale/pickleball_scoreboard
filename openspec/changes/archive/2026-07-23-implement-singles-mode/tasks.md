## 1. Firmware: state machine SINGLES branch

- [x] 1.1 Add `GameMode mode` parameter to `initGameState(GameState&, Side, GameMode)` in `firmware/include/game_state.h` / `firmware/src/game_state.cpp`, defaulting to `GameMode::DOUBLES` so existing call sites keep compiling.
- [x] 1.2 Add `GameMode mode` parameter to `handleReset(GameState&, Side, GameMode)`, same default.
- [x] 1.3 Factor the "full side-out" step (flip `servingSide` to the winner, reset `serverNumber` to 2) out of the existing Case B2 branch into a small shared helper, per design.md Decision 2.
- [x] 1.4 In `handleRallyWonBy`'s Case B, branch on `state.gameMode`: `DOUBLES` keeps the existing B1/B2 split calling the new helper for B2; `SINGLES` calls the same helper unconditionally (no B1 step), per Spec 01 Section 9a.
- [x] 1.5 Build firmware (`esp32dev` env) and fix any compile errors from the signature changes.

## 2. Firmware: BLE command parsing and notify

- [x] 2.1 Update `ble_command_parser.cpp` to parse the 3-byte reset command (`'0'` + side + mode) per Spec 02 Section 4a, dispatching `handleReset(state, side, mode)`.
- [x] 2.2 Update malformed-input handling: any `'0'`-prefixed command that isn't exactly 3 bytes with a valid side byte and a valid mode byte is ignored — remove the old 2-byte acceptance path entirely (breaking change, no fallback).
- [x] 2.3 Update `notifyState()` in `ble_service.cpp` to append the `gameMode` field (`'D'`/`'S'`) to the payload per design.md Decision 4: `<leftScore>,<rightScore>,<servingSide>,<serverNumber>,<gameEnded>,<gameMode>`.
- [x] 2.4 Build firmware and fix any compile errors.

## 3. Firmware: tests

- [x] 3.1 In `test_game_state.cpp`, add SINGLES Case B test(s): non-serving side wins with `gameMode = SINGLES` at both `serverNumber == 1` and `serverNumber == 2` starting points, asserting immediate full side-out in both cases (no B1 step).
- [x] 3.2 Add a SINGLES reset test asserting `initGameState`/`handleReset` correctly set `gameMode`.
- [x] 3.3 In `test_ble_command_parsing.cpp`, replace the existing 2-byte `"0L"`/`"0R"` reset tests with 3-byte equivalents (`"0LD"`, `"0RS"`, etc.) covering both sides and both modes.
- [x] 3.4 Add a test asserting the old 2-byte reset (`"0L"`) is now rejected as malformed (no state change, returns false).
- [x] 3.5 Add a test asserting an invalid mode byte (e.g. `"0LX"`) is rejected as malformed.
- [x] 3.6 Confirm no remaining test in the firmware suite asserts the old 2-byte reset format succeeds.
- [x] 3.7 Run native firmware tests and ensure they all pass. **Note:** `pio test -e native` itself cannot run in this environment — no `g++` is on PATH by default, and even after adding a local MSYS2 mingw64 `g++`, the `native` env's test files (all of them, including ones this change didn't touch) use the Arduino `setup()`/`loop()` pattern with no real `int main()`, so linking fails with an unrelated `undefined reference to WinMain` error. This predates this change. Worked around it by manually compiling+linking `test_game_state.cpp`/`test_ble_command_parsing.cpp` against `game_state.cpp`/`ble_command_parser.cpp` and Unity with a hand-written `main() { setup(); }` shim: all 17 game-state tests and all 22 BLE-parsing tests pass. Flagging the broken `pio test -e native` harness itself as a separate pre-existing issue worth fixing later.

## 4. Android: state machine SINGLES branch

- [x] 4.1 Add `mode: GameMode = GameMode.DOUBLES` parameter to `ScoreboardStateMachine.init(startingSide: Side, mode: GameMode)`.
- [x] 4.2 Add `mode: GameMode = GameMode.DOUBLES` parameter to `reset(startingSide: Side, mode: GameMode)`.
- [x] 4.3 Factor the "full side-out" step out of `applyRally`'s existing Case B2 branch into a small shared private helper, mirroring the firmware refactor.
- [x] 4.4 In `applyRally`'s Case B, branch on `state.gameMode`: `DOUBLES` keeps the existing B1/B2 split; `SINGLES` always applies the full side-out helper, per Spec 01 Section 9a.

## 5. Android: BLE client

- [x] 5.1 Update `BleClient`'s reset command writer (called from `MainActivity.sendReset`) to write 3 bytes: `'0'`, side byte, mode byte — using the new `CMD_RESET_SIDE_LEFT`/`CMD_RESET_SIDE_RIGHT` constants from 5.2 for the side byte, not `CMD_RALLY_WON_LEFT`/`CMD_RALLY_WON_RIGHT`.
- [x] 5.2 Add clearly-named mode byte constants (`CMD_MODE_DOUBLES` / `CMD_MODE_SINGLES` = `'D'`/`'S'`) and reset side-byte constants (`CMD_RESET_SIDE_LEFT` / `CMD_RESET_SIDE_RIGHT` = `'L'`/`'R'`) to `BleClient`'s companion object, alongside the existing command byte constants. The reset side byte must not reuse `CMD_RALLY_WON_LEFT`/`CMD_RALLY_WON_RIGHT` — those coincidentally share the same `'L'`/`'R'` values today, but Reset and Rally-Won are logically unrelated commands and shouldn't be coupled through shared constants.
- [x] 5.3 Update `parseStatePayload` to require 6 comma-separated fields (was 5), parsing the 6th as `gameMode` (`'D'`/`'S'`) into `ScoreboardState.gameMode`; any other field count or an invalid 6th character is malformed and returns `null`.

## 6. Android: UI wiring

- [x] 6.1 Update `ResetSideDialog` (or add a new dialog step) in `MainActivity.kt` to also ask "Singles or Doubles?" alongside the existing "Which side is 0-0-2?" prompt.
- [x] 6.2 Update `sendReset` in `MainActivity.kt` to accept the selected `GameMode` and write it as the 3-byte command via the new `BleClient` constants.
- [x] 6.3 Build the Android debug APK (`./gradlew assembleDebug`) and fix any compile errors.

## 7. Android: tests

- [x] 7.1 In `ScoreboardStateMachineTest.kt`, add SINGLES Case B test(s) mirroring the firmware tests in 3.1 (both `serverNumber == 1` and `== 2` starting points collapse to immediate side-out).
- [x] 7.2 Add a SINGLES reset test asserting `init`/`reset` correctly set `gameMode`.
- [x] 7.3 In `StatePayloadParserTest.kt`, update existing valid-payload tests to use 6-field payloads and add assertions on the parsed `gameMode`.
- [x] 7.4 Add a test asserting a 5-field (old-format) payload is now malformed (`parseStatePayload` returns `null`).
- [x] 7.5 Add tests for `gameMode` field values `'D'` and `'S'`, and for an invalid 6th character being malformed.
- [x] 7.6 Confirm no remaining test in the Android suite asserts the old 2-byte reset command or 5-field notify payload as valid.
- [x] 7.7 Run Android unit tests (`./gradlew testDebugUnitTest`) and ensure they all pass. (18 `StatePayloadParserTest` + 17 `ScoreboardStateMachineTest`, 0 failures. Required setting `JAVA_HOME` to Android Studio's bundled JBR since no system JDK/`JAVA_HOME` was configured in this environment.)

## 8. Final validation

- [x] 8.1 Run `pio run` in `firmware/` successfully. (`esp32dev` env: SUCCESS.)
- [x] 8.2 Run firmware native tests successfully. See note on task 3.7 — `pio test -e native` itself is broken by a pre-existing environment/harness gap unrelated to this change; verified via manual compile+link instead (17 + 22 tests, all passing).
- [x] 8.3 Run `./gradlew assembleDebug` in `android/` successfully.
- [x] 8.4 Run `./gradlew testDebugUnitTest` in `android/` successfully. (18 + 17 tests, 0 failures.)
- [x] 8.5 Review firmware and Android source comments referencing the old 2-byte reset / 5-field notify format and update them to match the new wire format. Remaining mentions of "two-byte"/"five-field" are deliberate historical-context comments explaining the breaking change, not stale claims that the old format still works.
- [x] 8.6 Confirm Spec 03 (display rendering) requires no changes for SINGLES mode — no new visual element was added, matching design.md Decision 5. Confirmed: no `gameMode`/`GameMode` reference exists anywhere in `display_render.cpp`, `display_render_logic.cpp`, or any Android rendering code.
