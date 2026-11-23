> Note: the wire format described here was amended by archived change 2026-07-23-implement-singles-mode: reset is now the three-byte sequence `'0' + <L|R> + <D|S>` and the state payload has six fields (gameMode appended).
> See docs/specs/02-ble-protocol.md Sections 4a and 5.

## Why

The scoring state machine is implemented and tested on the ESP32, but the BLE layer is still a stub on both firmware and Android. This change wires the real GATT service, command parsing, state notifications, and Android client so that button presses on the phone actually drive the authoritative game state on the board and the phone's preview stays in sync with the board.

## What Changes

- **Firmware (`firmware/src/ble_service.cpp` and `firmware/include/ble_service.h`)**
  - Implement the two-characteristic GATT structure from Spec 02 Section 3 (Command: Write, State: Notify) using the exact UUIDs specified there.
  - Advertise as `"PickleScore"` per Spec 02 Section 3.
  - Parse single-byte commands `'L'`, `'R'`, `'U'`, `'C'`, `'E'` and the two-byte reset sequence `'0'` + `'L'|'R'` per Spec 02 Section 4/4a, dispatching to the existing `game_state.cpp` handlers.
  - Serialize and Notify state after every state-changing command using the format from Spec 02 Section 5.
  - Send one Notify immediately on central connect (Spec 02 Section 6) and resume advertising on disconnect.
  - Silently ignore unrecognized command bytes and malformed reset sequences per Spec 02 Section 7.
  - Add a small, testable command-parsing harness and native unit tests for it.

- **Android (`android/app/src/main/java/com/example/pickleballscoreboard/ble/BleClient.kt`, `ui/ScoreboardScreen.kt`, `MainActivity.kt`)**
  - Replace/extend the `BleClient` stub with a real BLE central implementation: scan for `"PickleScore"`, connect, discover the Command and State characteristics, subscribe to State notifications, and write command bytes.
  - Parse incoming Notify payloads into `ScoreboardState` per Spec 02 Section 5; ignore malformed payloads per Spec 02 Section 7.
  - Wire the six control buttons to write the corresponding command bytes over BLE.
  - Keep the live UI as a "dumb renderer" driven only by BLE Notify payloads; the local `ScoreboardStateMachine` remains available for unit-test reference but is not used for live UI updates.
  - Add unit tests for the Notify payload parser.

- **Manual end-to-end test plan**
  - Provide a written checklist for verifying real ESP32 ↔ phone behavior by hand, since automated end-to-end BLE testing is not practical in this repo.

## Capabilities

### New Capabilities

- `ble-protocol`: The BLE GATT service, command encoding, state notification format, connection lifecycle, and error handling defined in `docs/specs/02-ble-protocol.md`. This OpenSpec capability spec captures the same requirements in delta format so the change can be tracked; the authoritative source of truth remains `docs/specs/02-ble-protocol.md`.

### Modified Capabilities

None. This change implements existing approved requirements without changing them.

## Impact

- `firmware/src/ble_service.cpp` / `firmware/include/ble_service.h` — major implementation.
- `firmware/test/` — new native unit tests for command parsing.
- `android/app/src/main/java/com/example/pickleballscoreboard/ble/BleClient.kt` — rewritten from stub into working client.
- `android/app/src/main/java/com/example/pickleballscoreboard/ui/ScoreboardScreen.kt` — button callbacks wired to BLE writes; preview state sourced from BLE.
- `android/app/src/main/java/com/example/pickleballscoreboard/MainActivity.kt` — may need minor lifecycle/connection setup.
- `android/app/src/test/java/com/example/pickleballscoreboard/` — new unit tests for Notify payload parsing.
