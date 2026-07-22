## Context

The previous `scaffold-firmware-and-app-2` change created empty stubs for the scoring state machine on both platforms:

- Firmware: `firmware/include/game_state.h` and `firmware/src/game_state.cpp` define `GameState`, `Side`, `GameMode`, and the transition function signatures.
- Android: `ScoreboardScreen.kt` has no state holder yet; button handlers are no-ops.

The authoritative rules live in `docs/specs/01-scoring-state-machine.md`. This change fills in the logic and proves both implementations with tests.

## Goals / Non-Goals

**Goals:**

- Implement Spec 01 Sections 4–5c logic in `firmware/src/game_state.cpp`.
- Implement a Kotlin state holder with identical semantics in the Android app.
- Add unit tests on both platforms covering Spec 01 Section 7 and every transition type.
- Keep the two implementations in lockstep: same input → same resulting state.

**Non-Goals:**

- Wiring state changes to BLE (Spec 02) or display rendering (Spec 03).
- Implementing SINGLES mode (Spec 01 Section 9a) or match-point/win detection (Spec 01 Section 8).
- Changing the public API of the existing `game_state.h` stubs.

## Decisions

1. **Firmware API stays as-is.** The existing `game_state.h` signatures already accept the 0-0-2 side parameter (`initGameState` and `handleReset`) and keep undo storage as an implementation detail, so no public API changes are needed.
2. **Undo storage is module-private.** The firmware will keep a single `GameState previousState` and a flag indicating whether a saved state exists. The Android state holder will keep an equivalent `previousState: GameState?` nullable property. Both satisfy Spec 01 Section 5a's single-step-only requirement.
3. **Reset is always active.** Because `handleReset` re-initializes the state from scratch, it works regardless of `gameEnded`, matching Spec 01 Section 5c.
4. **SINGLES field is structurally reserved only.** `gameMode` defaults to `DOUBLES` and is never read by the transition logic, per Spec 01 Section 9a.
5. **Firmware tests: native PlatformIO unit tests.** Rationale: PlatformIO supports `test/` directories with native or embedded runners. A native `test_game_state.cpp` under `firmware/test/` keeps the test cycle fast and does not require flashing hardware.
6. **Android tests: JUnit4 in `src/test/`.** Rationale: standard for Android/JVM, no Compose or instrumentation needed for pure state-machine tests.
7. **Every transition saves an undo snapshot, including END_GAME and RESET.** Rationale: Spec 01 Section 5a has been amended so UNDO applies uniformly to every input; `handleEndGame` and `handleReset` must snapshot the current state before mutating it, just like rally and switch transitions.

## Risks / Trade-offs

- [Risk] The two implementations can drift subtly (e.g., different handling of `serverNumber` after a side-out). → Mitigation: tests use the same test vectors on both platforms, and both are driven verbatim by Spec 01 Sections 4–5c.
- [Risk] PlatformIO native test runner may need extra dependency configuration. → Mitigation: if native linking is problematic, fall back to a small `main()`-based test harness guarded by a build flag, still checked into `firmware/test/`.
- [Risk] Case B1 (`serverNumber == 1`) is hard to reach in normal DOUBLES play because the game starts at server 2 and side-outs reset to 2. → Mitigation: explicitly construct a state with `serverNumber = 1` in tests to verify the transient behavior described in Spec 01 Section 5.

## Open Questions / Ambiguities

1. **Android state-holder packaging.** There is no existing `ViewModel` or dedicated state file. This change will add `com.example.pickleballscoreboard.state.ScoreboardState` (data class) and `com.example.pickleballscoreboard.state.ScoreboardStateMachine` (plain class). If the user prefers a `ViewModel`, that can be a thin wrapper added later when BLE/UI wiring happens.
2. **Score overflow.** Spec 01 does not define an upper score bound. The implementations will use `int` (C++) / `Int` (Kotlin) and allow natural overflow-free growth for any realistic game length.
