## Purpose

Defines the landscape Android controller that operators use courtside: direct score keys per team, a serve key that mirrors the board's arrows, and the game controls.

## ADDED Requirements

### Requirement: Sketch layout
The controller SHALL run in landscape only and SHALL place, left to right, the left team's `+` key, score and `−` key stacked vertically; a serve key; and the right team's `+` key, score and `−` key; with Undo, Switch Court, End Game and Reset in a bottom row.

#### Scenario: Layout order
- **WHEN** the controller is synchronized with a board
- **THEN** each `+` key is above its score, which is above its `−` key; the serve key is between the two team sections; and the bottom row reads Undo, Switch Court, End Game, Reset

#### Scenario: Touch targets and large text
- **WHEN** the screen is 569×320 dp with 1.35× font scale
- **THEN** every control remains on screen with at least a 48 dp target and the bottom row keys share one height

### Requirement: Board-authoritative display
Scores, arrows and the LED preview SHALL show only the board's reported Manual State; a tap SHALL NOT change any displayed value locally.

#### Scenario: Tap before Notify
- **WHEN** the operator taps Left plus
- **THEN** the displayed left score changes only after the board's Notify reports it

#### Scenario: Serve key mirrors the board
- **WHEN** the board reports L1, L2, R1, R2 or Singles
- **THEN** the serve key draws the same arrow count and direction as the LED display

### Requirement: Confirmed destructive actions
End Game and Reset SHALL require confirmation; Reset SHALL offer Doubles or Singles; while ended the End Game key SHALL become Resume and send Undo.

#### Scenario: End and resume
- **WHEN** the operator confirms End Game and then taps Resume
- **THEN** the controller sends `E` and then `U`, and play keys are disabled while the board reports the game ended

### Requirement: Synchronization gating
Without a subscribed, synchronized board the controller SHALL show no live score and no game controls.

#### Scenario: Link lost
- **WHEN** the connection is lost or not yet synchronized
- **THEN** the score keys, serve key and bottom controls are absent and any open game dialog closes

### Requirement: Stable controls while sending
Queued command feedback SHALL NOT move any control.

#### Scenario: Pending writes
- **WHEN** the number of queued commands changes between 0 and 32
- **THEN** the bounds of the preview, serve key, score keys and bottom keys are unchanged
