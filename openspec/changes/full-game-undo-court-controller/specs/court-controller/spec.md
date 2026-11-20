# Court controller delta

## Purpose

Provide a legible and tactile courtside controller that displays the state reported by the physical board.

## ADDED Requirements

### Requirement: Inline portrait preview and stacked controls
The controller SHALL show the exact portrait LED preview and match mode on the left. Rally Left, Rally Right, Undo, Switch Courts, End Game and Reset SHALL be stacked on the right, in that order. Connection status and the Board menu SHALL remain above the workspace. The Board menu SHALL expose the installed controller version even without a connection.

#### Scenario: Gameplay
- **WHEN** the board is synchronized
- **THEN** the rally keys correspond to the physical ends and score updates only from BLE

#### Scenario: Exhausted history
- **WHEN** extended state reports no recorded actions
- **THEN** Undo is disabled until an authoritative update reports history

#### Scenario: Unsafe commands
- **WHEN** the controller has no synchronized state
- **THEN** gameplay commands are disabled
