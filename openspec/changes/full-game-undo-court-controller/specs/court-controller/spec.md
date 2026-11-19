# Court controller delta

## Purpose

Provide a legible and tactile courtside controller that displays the state reported by the physical board.

## ADDED Requirements

### Requirement: Court-first controls
The controller SHALL show left and right scores, serving side and mode in a court view, expose the exact portrait LED preview, and provide distinct rally, Undo and court switch actions.

#### Scenario: Gameplay
- **WHEN** the board is synchronized
- **THEN** the rally keys correspond to the physical ends and score updates only from BLE

#### Scenario: Exhausted history
- **WHEN** extended state reports no recorded actions
- **THEN** Undo is disabled until an authoritative update reports history

#### Scenario: Unsafe commands
- **WHEN** the controller has no synchronized state
- **THEN** gameplay commands are disabled
