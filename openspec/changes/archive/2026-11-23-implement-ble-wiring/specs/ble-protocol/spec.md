> Note: the wire format described here was amended by archived change 2026-07-23-implement-singles-mode: reset is now the three-byte sequence `'0' + <L|R> + <D|S>` and the state payload has six fields (gameMode appended).
> See docs/specs/02-ble-protocol.md Sections 4a and 5.

## ADDED Requirements

### Requirement: GATT service exposes two characteristics
The ESP32 firmware SHALL advertise a single BLE service containing a Write-only Command characteristic and a Notify-only State characteristic with the UUIDs defined in Spec 02 Section 3. The Android app SHALL discover this service and both characteristics.

#### Scenario: Service discovery succeeds
- **WHEN** the ESP32 is powered and the Android app scans for and connects to the advertised `"PickleScore"` peripheral
- **THEN** the app discovers service `4fafc201-1fb5-459e-8fcc-c5c9c331914b` with Command characteristic `beb5483e-36e1-4688-b7f5-ea07361b26a8` and State characteristic `beb5483e-36e1-4688-b7f5-ea07361b26a9`

### Requirement: Peripheral advertises as PickleScore
The ESP32 firmware SHALL advertise the device name `"PickleScore"` whenever it is powered and not connected to a central, per Spec 02 Section 6.

#### Scenario: Advertising before connection
- **WHEN** the ESP32 boots and no central is connected
- **THEN** BLE scans show a peripheral named `"PickleScore"` advertising the service UUID `4fafc201-1fb5-459e-8fcc-c5c9c331914b`

### Requirement: Single-byte commands drive state machine inputs
The ESP32 firmware SHALL interpret a single written byte as a command per Spec 02 Section 4: `'L'` → `RALLY_WON_LEFT`, `'R'` → `RALLY_WON_RIGHT`, `'U'` → `UNDO`, `'C'` → `SWITCH_COURTS`, `'E'` → `END_GAME`. Each parsed command SHALL call the corresponding `game_state.cpp` handler.

#### Scenario: Rally won left command
- **WHEN** the firmware receives byte `'L'` on the Command characteristic
- **THEN** it calls `handleRallyWonLeft(state)` and notifies the updated state

#### Scenario: Undo command
- **WHEN** the firmware receives byte `'U'` on the Command characteristic
- **THEN** it calls `handleUndo(state)` and notifies the updated state

### Requirement: Undo with no previous state is silent
The ESP32 firmware SHALL treat a `'U'` command received when no previous state snapshot exists as a no-op: the game state SHALL remain unchanged and no Notify SHALL be sent, per Spec 01 Section 5a.

#### Scenario: First command after init is undo
- **WHEN** the firmware receives byte `'U'` immediately after `initGameState(state, Side::LEFT)` with no prior state-changing command
- **THEN** the game state remains `(0,0,L,2)` and no Notify is sent

#### Scenario: Undo immediately after a successful undo
- **WHEN** the firmware receives `'L'` followed by `'U'` followed by a second `'U'`
- **THEN** the first `'U'` restores the pre-rally state `(0,0,L,2)` and sends a Notify, and the second `'U'` leaves the state unchanged and sends no Notify

### Requirement: Reset command carries starting side parameter
The ESP32 firmware SHALL treat `'0'` as the first byte of a two-byte Reset command per Spec 02 Section 4a. The second byte SHALL be `'L'` or `'R'`, selecting which side starts as 0-0-2. The firmware SHALL call `handleReset(state, Side::LEFT)` or `handleReset(state, Side::RIGHT)` accordingly.

#### Scenario: Reset with left side 0-0-2
- **WHEN** the firmware receives bytes `'0'` then `'L'` on the Command characteristic
- **THEN** it calls `handleReset(state, Side::LEFT)` and notifies the reset state

#### Scenario: Reset with right side 0-0-2
- **WHEN** the firmware receives bytes `'0'` then `'R'` on the Command characteristic
- **THEN** it calls `handleReset(state, Side::RIGHT)` and notifies the reset state

### Requirement: State updates use comma-separated ASCII payload
The ESP32 firmware SHALL send state updates on the State characteristic using the format `<leftScore>,<rightScore>,<servingSide>,<serverNumber>,<gameEnded>` per Spec 02 Section 5, where `servingSide` is `'L'` or `'R'`, `serverNumber` is `'1'` or `'2'`, and `gameEnded` is `'0'` or `'1'`.

#### Scenario: Notify after rally left
- **WHEN** the state is leftScore=1, rightScore=0, servingSide=L, serverNumber=2, gameEnded=false
- **THEN** the notified payload is `"1,0,L,2,0"`

### Requirement: State notify is sent on every state change
The ESP32 firmware SHALL send a Notify payload immediately after processing any command that changes the game state.

#### Scenario: Score change triggers notify
- **WHEN** the firmware receives a valid command that changes the state
- **THEN** it sends one Notify payload with the new state before accepting further commands

### Requirement: Connect lifecycle immediately syncs state
On central connect, the ESP32 firmware SHALL send one state Notify with the current state. On central disconnect, it SHALL resume advertising without changing the displayed state, per Spec 02 Section 6.

#### Scenario: Reconnecting app receives current state
- **WHEN** an Android app connects to the ESP32 mid-game
- **THEN** the ESP32 immediately sends a Notify payload reflecting the current game state

#### Scenario: Disconnect does not stop the scoreboard
- **WHEN** the Android app disconnects
- **THEN** the ESP32 resumes advertising and continues displaying the current score unchanged

### Requirement: Malformed commands are ignored silently
The ESP32 firmware SHALL ignore unrecognized command bytes and malformed two-byte Reset sequences per Spec 02 Section 7. No state change SHALL occur and no Notify SHALL be sent for ignored input.

#### Scenario: Unrecognized byte is ignored
- **WHEN** the firmware receives byte `'X'` on the Command characteristic
- **THEN** the game state remains unchanged and no Notify is sent

#### Scenario: Malformed reset is ignored
- **WHEN** the firmware receives byte `'0'` not followed by `'L'` or `'R'`
- **THEN** the game state remains unchanged and no Notify is sent

### Requirement: Android app parses state payloads into ScoreboardState
The Android app SHALL parse every received Notify payload against Spec 02 Section 5 and update the displayed `ScoreboardState`. Any payload that does not parse cleanly SHALL be ignored.

#### Scenario: Valid payload updates UI
- **WHEN** the app receives payload `"3,5,R,2,0"`
- **THEN** the displayed state becomes leftScore=3, rightScore=5, servingSide=RIGHT, serverNumber=2, gameEnded=false

#### Scenario: Malformed payload is ignored
- **WHEN** the app receives payload `"3,R,2,0"` or any other non-conforming string
- **THEN** the displayed state is unchanged and the app does not crash

### Requirement: Android app is a dumb renderer
The Android app SHALL send button presses as BLE command bytes and display only the state received via BLE Notify. It SHALL NOT call the local `ScoreboardStateMachine` to compute live UI state.

#### Scenario: Button tap sends command only
- **WHEN** the user taps "Rally Left"
- **THEN** the app writes byte `'L'` to the Command characteristic and waits for the next Notify to update the UI

### Requirement: BLE security and multi-connection are out of scope
The v1 BLE implementation SHALL remain open (no pairing/encryption) and SHALL support only one central connection at a time, per Spec 02 Section 8.

#### Scenario: Single connection only
- **WHEN** one Android app is already connected
- **THEN** a second phone cannot simultaneously control the same ESP32
