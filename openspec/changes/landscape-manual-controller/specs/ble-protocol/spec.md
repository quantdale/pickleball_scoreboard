## ADDED Requirements

### Requirement: Manual command set
The ESP32 firmware SHALL accept exactly the Spec 02 Section 4 writes `+L`, `-L`, `+R`, `-R`, `S`, `C`, `E`, `U`, `0D` and `0S`, and SHALL ignore every other write, including the portrait rally bytes and three-byte reset, without state change, history or Notify.

#### Scenario: Direct adjustment command
- **WHEN** the firmware receives `+L`
- **THEN** it adds one point to the left score and notifies the new state

#### Scenario: Portrait command is malformed
- **WHEN** the firmware receives `L` or `0LD`
- **THEN** the game state and history remain unchanged and no Notify is sent

#### Scenario: Android writes only the specified bytes
- **WHEN** any Android control sends a command
- **THEN** the write is exactly one of the ten Spec 02 Section 4 values

### Requirement: Manual State stream
The firmware SHALL expose a Notify characteristic `beb5483e-36e1-4688-b7f5-ea07361b26ac` carrying `<left>,<right>,<side>,<server>,<ended>,<mode>,<undo>`, SHALL notify it whenever any of those fields changes and on initial synchronization after its CCCD write, and SHALL NOT expose `...26ab`.

#### Scenario: Undo availability changes without a visible change
- **WHEN** a same-position Reset is accepted from an empty history
- **THEN** Manual State is notified with Undo available while Legacy State is not notified

#### Scenario: Longest payload fits the default MTU
- **WHEN** the state is `99,99,R,2,1,D,1`
- **THEN** the Manual State payload is 15 bytes

### Requirement: Controller refuses portrait firmware
The Android controller SHALL subscribe only to Manual State and SHALL close the connection with an explanatory error, before any command write, when the service lacks Manual State but offers Legacy State.

#### Scenario: Portrait board discovered
- **WHEN** service discovery finds Legacy State but not Manual State
- **THEN** the controller reports that the board runs portrait firmware and sends no command

## REMOVED Requirements

### Requirement: Reset command carries starting side and game mode parameters
**Reason**: Reset now always starts at Left server 1; the operator sets the first server with the serve key.
**Migration**: Send `0D` or `0S` (Spec 02 Section 4).

### Requirement: Incomplete or malformed reset sequences are ignored
**Reason**: The three-byte reset no longer exists; all unlisted writes are malformed.
**Migration**: See the Manual command set requirement.
