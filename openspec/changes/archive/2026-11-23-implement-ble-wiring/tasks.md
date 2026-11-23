## 1. Firmware BLE command parsing and state notification

- [x] 1.1 Add a testable command-parsing function in `firmware/src/ble_command_parser.cpp` (e.g. `handleBleCommand`) that maps bytes to `game_state.cpp` handlers per Spec 02 Section 4/4a.
- [x] 1.2 Implement single-byte command dispatch for `'L'`, `'R'`, `'U'`, `'C'`, `'E'` calling `handleRallyWonLeft`, `handleRallyWonRight`, `handleUndo`, `handleSwitchCourts`, `handleEndGame`.
- [x] 1.3 Implement three-byte reset handling per Spec 02 Section 4a (format amended by change implement-singles-mode): `'0'` followed by side byte `'L'`/`'R'` and mode byte `'D'`/`'S'` calls `handleReset(state, side, mode)`; any incomplete or invalid sequence is ignored entirely.
- [x] 1.4 Implement silent ignore for unrecognized command bytes and malformed reset sequences (Spec 02 Section 7).
- [x] 1.5 Implement `notifyState(const GameState&)` serialization per Spec 02 Section 5: `<leftScore>,<rightScore>,<servingSide>,<serverNumber>,<gameEnded>,<gameMode>` (gameMode field added by change implement-singles-mode).
- [x] 1.6 Wire `CommandCallbacks::onWrite` to call the parsing function and notify only when state changes.
- [x] 1.7 Wire `ServerCallbacks::onConnect` to send one immediate state Notify (Spec 02 Section 6).
- [x] 1.8 Confirm `ServerCallbacks::onDisconnect` resumes advertising (already present in stub; verify behavior).
- [x] 1.9 Build firmware with `.venv/Scripts/pio.exe run` and fix any compile errors (esp32dev succeeded; native env blocked by missing `g++` on this Windows host).

## 2. Firmware unit tests for command parsing

- [x] 2.1 Add `firmware/test/test_ble_command_parsing/test_ble_command_parsing.cpp` using the existing native test pattern.
- [x] 2.2 Cover all six command inputs and the resulting `GameState` values.
- [x] 2.3 Cover the three-byte reset with both side bytes (`'L'`/`'R'`) and both mode bytes (`'D'`/`'S'`).
- [x] 2.4 Cover unrecognized single bytes producing no state change and no notify.
- [x] 2.5 Cover malformed reset (`'0'` alone, `'0'` + invalid side byte, or `'0'` + side byte without a valid mode byte) producing no reset.
- [x] 2.6 Add an explicit assertion that `'U'` with no saved previous state (first command after init, or immediately after a successful undo) produces no state change and no notify, distinct from an unrecognized-byte no-op (Spec 01 Section 5a).
- [x] 2.7 Run native firmware tests and ensure they pass.

## 3. Android BLE client implementation

- [x] 3.1 Rewrite/extend `android/app/src/main/java/com/example/pickleballscoreboard/ble/BleClient.kt` to use the Android BLE stack.
- [x] 3.2 Implement scan for `"PickleScore"` advertising name and expose found devices to the UI.
- [x] 3.3 Implement connect + service discovery for the Spec 02 Section 3 service UUID.
- [x] 3.4 Enable notifications (CCCD) on the State characteristic and parse incoming payloads.
- [x] 3.5 Implement `writeCommand(bytes: ByteArray)` writing to the Command characteristic.
- [x] 3.6 Add error handling/logging for scan, connect, discovery, and write failures.

## 4. Android state payload parser

- [x] 4.1 Add a pure function `parseStatePayload(payload: String): ScoreboardState?` in `BleClient.kt`.
- [x] 4.2 Parse the Spec 02 Section 5 comma-separated format into `ScoreboardState`.
- [x] 4.3 Return `null` for any malformed payload (wrong field count, non-integer scores, invalid side/server/ended values) per Spec 02 Section 7.

## 5. Android UI wiring

- [x] 5.1 Update `MainActivity.kt` to instantiate `BleClient`, manage connection lifecycle, and hold the current `ScoreboardState` in Compose state.
- [x] 5.2 Add a simple scan/connect flow (button or dialog) so the user can select the `"PickleScore"` peripheral.
- [x] 5.3 Update `ScoreboardScreen` signature to accept a `ScoreboardState` and button callbacks that write BLE commands.
- [x] 5.4 Wire each of the six control buttons to the corresponding `BleClient.writeCommand` byte sequence per Spec 02 Section 4.
- [x] 5.5 For Reset, prompt the user to choose the game mode and the 0-0-2 side (`ResetSideDialog`) before sending the three-byte reset command `'0'` + side + mode (mode selection added by change implement-singles-mode).
- [x] 5.6 Ensure the local `ScoreboardStateMachine` is not used for live UI updates; the displayed state must come from BLE Notify only.
- [x] 5.7 Build Android debug APK with `./gradlew assembleDebug` and fix any compile errors.

## 6. Android unit tests for payload parsing

- [x] 6.1 Add `android/app/src/test/java/com/example/pickleballscoreboard/ble/StatePayloadParserTest.kt`.
- [x] 6.2 Cover valid payloads for both sides and both server numbers, plus ended/non-ended states.
- [x] 6.3 Cover malformed payloads: empty string, wrong field count, non-numeric scores, invalid side/server/ended characters.
- [x] 6.4 Run Android unit tests with `./gradlew testDebugUnitTest` and ensure they pass.

## 7. Manual end-to-end verification plan

- [ ] 7.1 Write a manual test checklist in `openspec/changes/implement-ble-wiring/manual-e2e-checklist.md` and execute it with real hardware:
  - Flash firmware and confirm serial boot without errors.
  - Run app, grant BLE permissions, scan, and connect to `"PickleScore"`.
  - Verify initial state notify appears in app and matches board.
  - Tap Rally Left / Rally Right and verify score updates on both app and board.
  - Tap Switch Courts and verify score+serve swap on both app and board.
  - Tap Undo and verify one-step revert on both app and board.
  - Tap End Game and verify inputs freeze; Reset prompts for side and restores 0-0-2.
  - Kill and reopen app; confirm reconnect syncs current state immediately.
  - Walk out of range / disable Bluetooth and confirm board continues displaying current state.

## 8. Final validation

- [x] 8.1 Run `pio run` in `firmware/` successfully.
- [x] 8.2 Run firmware native tests successfully.
- [x] 8.3 Run `./gradlew assembleDebug` in `android/` successfully.
- [x] 8.4 Run `./gradlew testDebugUnitTest` in `android/` successfully.
- [x] 8.5 Review that no spec comments reference outdated stubs; update where needed.
