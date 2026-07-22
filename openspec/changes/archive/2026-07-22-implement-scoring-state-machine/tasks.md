## 1. Firmware state machine

- [x] 1.1 Implement `initGameState(GameState&, Side)` per Spec 01 Section 4 (scores 0, `serverNumber = 2`, `gameMode = DOUBLES`, `gameEnded = false`).
- [x] 1.2 Implement `handleRallyWonLeft(GameState&)` and `handleRallyWonRight(GameState&)` per Spec 01 Section 5 (Case A / Case B1 / Case B2).
- [x] 1.3 Implement `handleSwitchCourts(GameState&)` per Spec 01 Section 5 (swap scores, flip serving side).
- [x] 1.4 Implement `handleEndGame(GameState&)` per amended Spec 01 Section 5b (`gameEnded = true`), saving a snapshot beforehand so UNDO can restore the live state.
- [x] 1.5 Implement `handleReset(GameState&, Side)` per amended Spec 01 Section 5c (re-initialize with new 0-0-2 side, works while ended), saving a snapshot beforehand so UNDO can restore the old game state.
- [x] 1.6 Implement `handleUndo(GameState&)` per Spec 01 Section 5a (restore the single saved snapshot and clear it; no redo-of-undo).
- [x] 1.7 Add a uniform `saveSnapshot()` helper and internal undo-state storage to `firmware/src/game_state.cpp` so every transition (rally, switch, end game, reset) snapshots before mutating, without changing `game_state.h` public API.

## 2. Firmware tests

- [x] 2.1 Add `firmware/test/test_game_state/test_game_state.cpp` covering Spec 01 Section 7 and one test per transition type.
- [x] 2.2 Add tests proving UNDO after END_GAME restores the pre-frozen state and UNDO after RESET restores the old game's state.
- [x] 2.3 Run firmware tests with PlatformIO (`pio test` or native runner) and confirm all pass.

## 3. Android state machine

- [x] 3.1 Create `com.example.pickleballscoreboard.state.ScoreboardState` data class with the same fields as Spec 01 Section 2.
- [x] 3.2 Create `com.example.pickleballscoreboard.state.ScoreboardStateMachine` class exposing `init(side)`, `rallyWonLeft()`, `rallyWonRight()`, `switchCourts()`, `undo()`, `endGame()`, and `reset(side)`.
- [x] 3.3 Implement the transition logic so it matches the firmware implementation exactly, with every transition saving a snapshot before mutating state.
- [x] 3.4 Keep single-step undo as a nullable `previousState` property inside the state machine.

## 4. Android tests

- [x] 4.1 Add `android/app/src/test/java/com/example/pickleballscoreboard/state/ScoreboardStateMachineTest.kt` covering Spec 01 Section 7 and one test per transition type.
- [x] 4.2 Add tests proving UNDO after `endGame()` restores the pre-frozen state and UNDO after `reset()` restores the old game's state.
- [x] 4.3 Run Android unit tests (`./gradlew test`) and confirm all pass.

## 5. Validation

- [x] 5.1 Confirm `firmware/src/game_state.cpp` compiles with `pio run`.
- [x] 5.2 Confirm `android/` compiles with `./gradlew assembleDebug`.
