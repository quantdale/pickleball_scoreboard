# Portrait controller delta

## Purpose

Operate the scoreboard in portrait without guessing whether a command changed the score.

## ADDED Requirements

### Requirement: Right-thumb controls and authoritative feedback

The controller SHALL present the portrait preview to the left of stacked controls, gate play on BLE readiness and confirm Reset and End Game before dispatch.

#### Scenario: A rally is submitted

- **WHEN** the ready controller submits a rally
- **THEN** the preview SHALL remain unchanged until a valid scoreboard notification arrives, and transport feedback SHALL not imply a scoring acknowledgment.

#### Scenario: The link is lost

- **WHEN** the ready session fails
- **THEN** gameplay SHALL be disabled until a replacement session synchronizes, and pending commands SHALL not be replayed.
