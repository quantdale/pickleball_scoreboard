# ble-protocol Specification

## Purpose
Defines the BLE command and state Notify protocol between the Android app and the ESP32 firmware per docs/specs/02: single-byte commands, the three-byte Reset, and the six-field state payload.

## Requirements
### Requirement: Reset command carries starting side and game mode parameters
The ESP32 firmware SHALL treat `'0'` as the first byte of a three-byte Reset command per Spec 02 Section 4a. The second byte SHALL be `'L'` or `'R'`, selecting which side starts as 0-0-2. The third byte SHALL be `'D'` or `'S'`, selecting DOUBLES or SINGLES. The firmware SHALL call `handleReset(state, side, mode)` with the corresponding `Side` and `GameMode`.

#### Scenario: Reset with left side 0-0-2, doubles
- **WHEN** the firmware receives bytes `'0'`, `'L'`, `'D'` on the Command characteristic
- **THEN** it calls `handleReset(state, Side::LEFT, GameMode::DOUBLES)` and notifies the reset state

#### Scenario: Reset with right side 0-0-2, singles
- **WHEN** the firmware receives bytes `'0'`, `'R'`, `'S'` on the Command characteristic
- **THEN** it calls `handleReset(state, Side::RIGHT, GameMode::SINGLES)` and notifies the reset state

### Requirement: Incomplete or malformed reset sequences are ignored
The ESP32 firmware SHALL ignore a `'0'` command that is not followed by exactly a valid side byte (`'L'`/`'R'`) and a valid mode byte (`'D'`/`'S'`), per Spec 02 Section 7. This includes the previously-valid two-byte reset format, which is now itself malformed input. No state change SHALL occur and no Notify SHALL be sent.

#### Scenario: Two-byte reset is now malformed
- **WHEN** the firmware receives bytes `'0'`, `'L'` with no third byte
- **THEN** the game state remains unchanged and no Notify is sent

#### Scenario: Invalid mode byte is malformed
- **WHEN** the firmware receives bytes `'0'`, `'L'`, `'X'`
- **THEN** the game state remains unchanged and no Notify is sent

#### Scenario: Android app never writes the old two-byte reset format
- **WHEN** the Android app sends a Reset command
- **THEN** it always writes exactly three bytes: `'0'`, the side byte, and the mode byte

### Requirement: State updates include the game mode field
The ESP32 firmware SHALL append a `gameMode` field to the state Notify payload per Spec 02 Section 5: `<leftScore>,<rightScore>,<servingSide>,<serverNumber>,<gameEnded>,<gameMode>`, encoded as `'D'` (DOUBLES) or `'S'` (SINGLES). The Android app SHALL parse this field into `ScoreboardState.gameMode` and SHALL treat a payload with any field count other than 6 as malformed.

#### Scenario: Notify includes game mode
- **WHEN** the state is leftScore=1, rightScore=0, servingSide=L, serverNumber=2, gameEnded=false, gameMode=SINGLES
- **THEN** the notified payload is `"1,0,L,2,0,S"`

#### Scenario: App parses game mode from a valid payload
- **WHEN** the app receives payload `"3,5,R,2,0,D"`
- **THEN** the displayed state has `gameMode == GameMode.DOUBLES`

#### Scenario: Old five-field payload is now malformed
- **WHEN** the app receives a payload with only five comma-separated fields (the pre-SINGLES format)
- **THEN** the app treats it as malformed and ignores it, per Spec 02 Section 7
