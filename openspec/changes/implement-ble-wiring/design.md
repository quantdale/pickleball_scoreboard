## Context

The ESP32 firmware currently has:
- A fully implemented and tested Spec 01 state machine in `firmware/src/game_state.cpp`.
- A BLE GATT scaffold in `firmware/src/ble_service.cpp` that sets up the correct service and two characteristics, but leaves command parsing, state serialization, and lifecycle hooks as stubs.

The Android app currently has:
- A matching `ScoreboardState` data class and a `ScoreboardStateMachine` implementation (from the prior scoring change).
- A `BleClient` stub with the correct UUIDs and command-byte constants, but no actual BLE stack usage.
- A `ScoreboardScreen` with six no-op buttons and a blank preview canvas.

This change connects the two sides via the Spec 02 wire format.

## Goals / Non-Goals

**Goals:**
- Make the ESP32 the authoritative BLE peripheral that accepts commands and pushes state updates.
- Make the Android app a dumb renderer that sends commands and displays whatever state the ESP32 reports.
- Keep all behavior aligned with Spec 02 (BLE protocol) and Spec 01 (state machine).
- Add isolated unit tests for the parsing logic on both platforms.
- Provide a manual end-to-end checklist for real hardware verification.

**Non-Goals:**
- Pixel rendering of scores/arrows on either platform (Spec 03).
- SINGLES mode support.
- Match-point or win detection.
- BLE pairing, bonding, encryption, or multiple simultaneous connections (already out of scope per Spec 02 Section 8).
- Replacing the existing local `ScoreboardStateMachine` entirely — it stays as a reference/test utility.

## Decisions

### 1. Firmware command parsing will be a testable free function

The existing `CommandCallbacks::onWrite` in `ble_service.cpp` will delegate to a new free function, e.g. `void handleBleCommand(const std::string& bytes, GameState& state)`. This function:
- Interprets the first byte per Spec 02 Section 4.
- For `'0'`, checks the second byte and dispatches `handleReset(state, Side::LEFT|RIGHT)` only when it is `'L'` or `'R'`.
- Silently ignores everything else per Spec 02 Section 7.
- Returns whether the state changed, so the caller can decide whether to call `notifyState(state)`.

**Rationale:** Keeps the BLE stack (Arduino BLECharacteristic callbacks) out of unit tests. Tests can feed byte arrays directly into the parsing function and inspect the resulting `GameState`.

### 2. State notification is sent only when state actually changes

After parsing a command, `ble_service.cpp` calls `notifyState(state)` only if the command was recognized and caused a state transition. Unrecognized bytes produce no notify (Spec 02 Section 7).

**Rationale:** Matches the spec exactly and avoids noise on the Notify characteristic.

### 3. Connection lifecycle hooks send one immediate Notify and resume advertising

`ServerCallbacks::onConnect` will call `notifyState(currentState)` once. `ServerCallbacks::onDisconnect` will call `BLEDevice::startAdvertising()`.

**Rationale:** Satisfies Spec 02 Section 6. The physical scoreboard continues to display the current state regardless of connection state because display rendering is separate from BLE.

### 4. State serialization is centralized in `ble_service.cpp`

`notifyState(const GameState& state)` will format the payload as:
```
"<leftScore>,<rightScore>,<servingSide>,<serverNumber>,<gameEnded>"
```
with `servingSide` encoded as `'L'` or `'R'` and `gameEnded` as `'0'` or `'1'`.

**Rationale:** Single source of truth for the wire format, easy to audit against Spec 02 Section 5.

### 5. Android `BleClient` becomes a real central with a small public surface

The existing stub will be extended/replaced to expose:
- `startScan(onFound: (BluetoothDevice) -> Unit, onError: (Throwable) -> Unit)`
- `connect(device: BluetoothDevice, onState: (ScoreboardState) -> Unit, onError: (Throwable) -> Unit)` — discovers service, enables notifications, and starts emitting parsed state.
- `disconnect()`
- `writeCommand(bytes: ByteArray)` — writes to the Command characteristic.

**Rationale:** Hides the Android BLE stack behind a small API that maps cleanly to the Spec 02 operations.

### 6. App UI state is driven solely by BLE Notify payloads

`MainActivity` will hold a `ScoreboardState` in Compose state. `ScoreboardScreen` will receive this state plus button callbacks that call `bleClient.writeCommand(...)`.

`ScoreboardStateMachine` will not be called for live updates. It remains in the source tree for reference and for any future local unit tests, but the production data path is:
```
Button tap → BleClient.writeCommand → ESP32 → Notify → BleClient parser → Compose state
```

**Rationale:** Enforces the Spec 02 architecture — the ESP32 owns state, the app only renders it. This prevents drift between preview and physical board.

### 7. Notify payload parsing is isolated and unit-tested

A pure function `fun parseStatePayload(payload: String): ScoreboardState?` will parse Section 5 payloads and return `null` for anything malformed. `BleClient` will call it and drop `null` results.

**Rationale:** Allows exhaustive unit testing without a BLE connection or emulator.

### 8. Reset requires explicit side selection on Android

The Reset button will open a simple dialog (or use an existing side selector) asking "Which side is 0-0-2?" and then send the two-byte `'0'` + `'L'|'R'` sequence.

**Rationale:** Matches Spec 02 Section 4a and Spec 01 Section 4. Avoids guessing a default side.

### 9. Firmware native tests use the existing `firmware/test/` pattern

A new test file will exercise `handleBleCommand` with sequences covering all six inputs, the two-byte reset, unrecognized bytes, and malformed reset. It will also include an explicit assertion that `'U'` with no saved previous state — both as the first command after init and immediately after a successful undo — leaves the state unchanged and does not trigger a notify, confirming Spec 01 Section 5a's "you cannot undo an undo" behavior over BLE. The actual BLE notify/advertise behavior will be manually verified.

**Rationale:** The Arduino BLE stack cannot easily be mocked in the native test environment; the parsing logic is the valuable part to lock down automatically.

## Risks / Trade-offs

- **[Risk] Android BLE scan/connect requires runtime permissions and can behave differently across manufacturers.** → Mitigation: request permissions in `MainActivity`, surface connection errors to the UI, and document the manual test checklist so Dale can verify on the target phone.
- **[Risk] The Android `BleClient` stub API changes shape significantly.** → Mitigation: This is expected and noted in the proposal. The new API is small and caller code in `MainActivity`/`ScoreboardScreen` is minimal.
- **[Risk] State notify on connect may race with the app enabling CCCD notifications.** → Mitigation: The ESP32 sends immediately on connect; the Android client should subscribe as part of service discovery. If the first notify is missed, any subsequent button press will re-sync. This is acceptable per Spec 02 Section 6 and can be verified manually.
- **[Risk] Reusing the local `ScoreboardStateMachine` by accident.** → Mitigation: Code review / explicit architecture note in this design. The live UI must not call `rallyWonLeft()` etc. on the local machine.

## Open Questions

- Should the Android app reconnect automatically when the ESP32 comes back in range? For v1, manual connect via a "Scan / Connect" button is simplest and avoids background-BLE complexity.
- Should the firmware keep advertising while connected? Spec 02 Section 6 says advertise when "not already connected," so advertising stops while connected. This matches the default Arduino BLE behavior.
