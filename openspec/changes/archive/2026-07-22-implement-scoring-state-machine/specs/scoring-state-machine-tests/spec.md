## ADDED Requirements

### Requirement: Firmware tests cover all Spec 01 transitions
The firmware test suite SHALL verify the scoring state machine implementation in `firmware/src/game_state.cpp` against the transition rules in `docs/specs/01-scoring-state-machine.md`.

#### Scenario: Worked example sequence
- **WHEN** the test reproduces the three-step sequence from Spec 01 Section 7 starting with Left as 0-0-2
- **THEN** the state after each step matches `(1,0,L,2)`, `(1,0,R,2)`, and `(1,0,L,2)` respectively

#### Scenario: Case A — serving side wins rally
- **WHEN** the serving side wins a rally
- **THEN** that side's score increments by 1 and `servingSide`/`serverNumber` remain unchanged

#### Scenario: Case B1 — non-serving side wins when serverNumber is 1
- **GIVEN** `serverNumber == 1` and the non-serving side wins the rally
- **WHEN** the rally transition is applied
- **THEN** scores do not change, `servingSide` does not change, and `serverNumber` becomes 2

#### Scenario: Case B2 — non-serving side wins when serverNumber is 2
- **GIVEN** `serverNumber == 2` and the non-serving side wins the rally
- **WHEN** the rally transition is applied
- **THEN** scores do not change, `servingSide` flips to the winning side, and `serverNumber` remains 2

#### Scenario: Switch courts
- **WHEN** `SWITCH_COURTS` is applied
- **THEN** `leftScore` and `rightScore` swap, `servingSide` flips, and `serverNumber` is unchanged

#### Scenario: Single-step undo
- **GIVEN** a transition has just changed the state
- **WHEN** `UNDO` is applied
- **THEN** the state reverts to the pre-transition state and a second consecutive `UNDO` has no effect

#### Scenario: Undo after end game
- **GIVEN** a live game state and `END_GAME` has just been applied
- **WHEN** `UNDO` is applied
- **THEN** the state reverts to the live game state that existed immediately before `END_GAME`

#### Scenario: Undo after reset
- **GIVEN** an in-progress game state and `RESET` has just been applied with a new 0-0-2 side
- **WHEN** `UNDO` is applied
- **THEN** the state reverts to the old game's state that existed immediately before `RESET`

#### Scenario: End game freeze
- **GIVEN** `gameEnded == true`
- **WHEN** any of `RALLY_WON_LEFT`, `RALLY_WON_RIGHT`, `SWITCH_COURTS`, or `UNDO` are applied
- **THEN** the state does not change

#### Scenario: Reset recovers from ended state
- **GIVEN** `gameEnded == true`
- **WHEN** `RESET` is applied with a new 0-0-2 side
- **THEN** the state becomes the Spec 01 Section 4 initial state for that side

### Requirement: Android tests cover all Spec 01 transitions
The Android test suite SHALL verify the Kotlin state holder with the same coverage as the firmware test suite.

#### Scenario: Mirrored worked example sequence
- **WHEN** the Android test reproduces the three-step sequence from Spec 01 Section 7
- **THEN** the resulting state after each step matches the firmware results

#### Scenario: Mirrored transition coverage
- **WHEN** the Android test exercises Case A, Case B1, Case B2, switch courts, undo, end-game freeze, and reset
- **THEN** each resulting state matches the corresponding firmware state

#### Scenario: Mirrored undo after end game
- **WHEN** the Android test applies `endGame()` followed immediately by `undo()`
- **THEN** the state matches the firmware state after the same sequence

#### Scenario: Mirrored undo after reset
- **WHEN** the Android test applies `reset()` followed immediately by `undo()`
- **THEN** the state matches the firmware state after the same sequence

### Requirement: Cross-platform equivalence
For any valid sequence of Spec 01 inputs, the firmware C++ state machine and the Android Kotlin state holder SHALL produce the same final `GameState`.

#### Scenario: Random walk equivalence
- **WHEN** an arbitrary valid input sequence is applied to both implementations
- **THEN** all corresponding fields (`leftScore`, `rightScore`, `servingSide`, `serverNumber`, `gameEnded`) are equal
