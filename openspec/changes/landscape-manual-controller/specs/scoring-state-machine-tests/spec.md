## ADDED Requirements

### Requirement: Firmware tests cover the manual scoring model
The firmware test suite SHALL verify the manual scoring state in `firmware/src/game_state.cpp` against `docs/specs/01-scoring-state-machine.md`, including direct score adjustments, the serve cycle in both modes, Switch Courts, End Game, Reset and Undo history.

#### Scenario: Worked example sequence
- **WHEN** the test applies the Spec 01 Section 7 sequence from boot
- **THEN** the state after each listed input matches the table, ending at `0,0,R,1,0,S`

#### Scenario: Direct adjustments
- **WHEN** `SCORE_UP` or `SCORE_DOWN` is applied to one side
- **THEN** only that side's score changes by one, the serving indicator is unchanged, and a score at 99 or 0 ignores the input without recording history

#### Scenario: Doubles serve cycle
- **WHEN** `NEXT_SERVE` is applied repeatedly from `L1` in Doubles
- **THEN** the indicator visits exactly `L2`, `R1`, `R2`, `L1` in that order

#### Scenario: Singles serve cycle
- **WHEN** `NEXT_SERVE` is applied in Singles
- **THEN** the serving side alternates and `serverNumber` stays 1

#### Scenario: Switch courts
- **WHEN** `SWITCH_COURTS` is applied
- **THEN** `leftScore` and `rightScore` swap, `servingSide` flips, and `serverNumber` is unchanged

#### Scenario: End game freeze and resume
- **GIVEN** `gameEnded == true`
- **WHEN** score, serve, switch or End Game inputs are applied
- **THEN** the state and history do not change, and a following `UNDO` restores the live position

#### Scenario: Reset recovers from ended state
- **GIVEN** `gameEnded == true`
- **WHEN** `RESET` is applied with a mode
- **THEN** the state becomes `0,0,L,1,0` in that mode and a following `UNDO` restores the ended game

#### Scenario: Undo restores every prior position
- **WHEN** a long random sequence of accepted inputs is undone
- **THEN** each Undo restores exactly the position before the matching input, until boot

#### Scenario: Full history drops the oldest entry
- **WHEN** more than 2,048 inputs are accepted
- **THEN** every input is still applied, history depth stays at 2,048, and the newest 2,048 positions unwind in order

### Requirement: Android tests mirror the manual scoring model
The Android test oracle SHALL mirror Spec 01 independently and SHALL replay the shared fixture that the firmware also replays.

#### Scenario: Shared fixture replay
- **WHEN** the Android oracle replays `shared/scoring_fixtures/manual-controller.inc`
- **THEN** every step's Legacy State projection, Undo availability and change flag matches the fixture, and the Android parser accepts the matching Manual State payload

#### Scenario: Mirrored serve cycle and history
- **WHEN** the Android oracle applies the serve cycle and more than 2,048 inputs
- **THEN** its states match the firmware rules for the cycle, eviction and Undo

## MODIFIED Requirements

### Requirement: Cross-platform equivalence
For the shared fixtures, the firmware C++ state and the Android Kotlin oracle SHALL produce identical states after every step.

#### Scenario: Random walk equivalence
- **WHEN** the seeded 600-step walk in `shared/scoring_fixtures/manual-random-walk.inc`, recorded from the firmware code and including malformed portrait commands, is replayed by both implementations
- **THEN** all corresponding fields (`leftScore`, `rightScore`, `servingSide`, `serverNumber`, `gameMode`, `gameEnded`, Undo availability) are equal at every step

#### Scenario: Shared fixture equivalence
- **WHEN** both implementations replay the same fixture
- **THEN** all corresponding fields (`leftScore`, `rightScore`, `servingSide`, `serverNumber`, `gameMode`, `gameEnded`, Undo availability) are equal at every step

## REMOVED Requirements

### Requirement: Firmware tests cover all Spec 01 transitions
**Reason**: Spec 01 no longer has rally, side-out or opening-server transitions; scores and serving are set directly.
**Migration**: See "Firmware tests cover the manual scoring model".

### Requirement: Android tests cover all Spec 01 transitions
**Reason**: The Kotlin oracle now mirrors the manual model instead of rally scoring.
**Migration**: See "Android tests mirror the manual scoring model".
