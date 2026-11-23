## Purpose

Lets operators raise or lower the physical scoreboard brightness from the
controller within firmware-enforced limits, independently of scoring.

## ADDED Requirements

### Requirement: Brightness levels
The firmware SHALL offer levels 1 to a build-time cap of at most 8, mapped to driver values 12, 16, 24, 32, 48, 64, 96 and 128 out of 255. Level 1 (12/255) SHALL be the default. The default cap SHALL be 4, documented as uncertified until the physical acceptance test passes.

#### Scenario: First boot
- **WHEN** the board boots with no stored level
- **THEN** the display is driven at 12/255 and the board reports `B,1,4`

#### Scenario: Each level is distinct
- **WHEN** the ladder is checked against the pinned driver's arithmetic
- **THEN** every level lights strictly more than the one below it

### Requirement: Brightness command
The firmware SHALL accept `B` followed by one digit from 1 to the cap on the command characteristic, apply it without restarting or redrawing, and ignore any other `B` write without change or Notify.

#### Scenario: Valid level
- **WHEN** the board at level 1 receives `B3`
- **THEN** the display brightness changes at once and the board notifies `B,3,4`

#### Scenario: Above the cap
- **WHEN** the board receives `B5` with a cap of 4
- **THEN** brightness is unchanged and nothing is notified

#### Scenario: Same level
- **WHEN** the board at level 2 receives `B2`
- **THEN** nothing is notified

### Requirement: Independence from scoring
A brightness command SHALL NOT change any game state field or Undo history, and no scoring input, Undo, End Game or Reset SHALL change brightness.

#### Scenario: Undo after a brightness change
- **WHEN** the operator scores, raises brightness, then taps Undo
- **THEN** the score reverts and the brightness stays at the raised level

### Requirement: Brightness report
The firmware SHALL send `B,<level>,<cap>` on the Manual State stream after the state payload on every initial synchronization and after every accepted change, and never on Legacy State.

#### Scenario: Reconnect
- **WHEN** a controller reconnects and enables Manual State notifications
- **THEN** it receives the current state and then the retained brightness level

#### Scenario: Older controller
- **WHEN** Android 2.0.0 receives `B,2,4`
- **THEN** it ignores the payload and remains synchronized

### Requirement: Persistence
The firmware SHALL store the level in NVS after it has been unchanged for 3 seconds, not during a maintenance session, and SHALL restore it before the first frame, using level 1 for a missing or invalid record and the cap for a stored level above it.

#### Scenario: Restart
- **WHEN** the level was 3 for more than 3 seconds and the board restarts
- **THEN** the boot frame is drawn at level 3

### Requirement: Controller brightness control
The Android controller SHALL show a brightness control only in its Board menu, only while synchronized, displaying the board-reported level. Rapid taps SHALL step from the newest unacknowledged request within the reported range, keep at most one unsent brightness write, and send every queued scoring write first. A synchronized board that sends no report SHALL be shown as needing an update.

#### Scenario: Rapid taps
- **WHEN** the operator taps Brighter five times quickly from level 1 with cap 4
- **THEN** at most two brightness writes are sent and the board settles at level 4

#### Scenario: Older board
- **WHEN** the controller synchronizes with `court-landscape-001`
- **THEN** the Board menu shows "Needs board update" and no brightness write is sent
