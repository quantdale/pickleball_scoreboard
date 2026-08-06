package com.example.pickleballscoreboard.state

// Scoring state machine implementation.
// See docs/specs/01-scoring-state-machine.md for the authoritative rules.
//
// The constructor overload accepting an initial state is intended for tests
// that need a specific starting state; production code should use the no-arg
// constructor plus init()/reset().
class ScoreboardStateMachine(initialState: ScoreboardState = ScoreboardState()) {
    var state: ScoreboardState = initialState
        private set

    private var previousState: ScoreboardState? = null

    // Spec 01 Section 4, Section 9b.
    fun init(startingSide: Side, mode: GameMode = GameMode.DOUBLES) {
        state = ScoreboardState(servingSide = startingSide, gameMode = mode)
    }

    // Spec 01 Section 5 RALLY_WON_LEFT.
    fun rallyWonLeft() = applyRally(Side.LEFT)

    // Spec 01 Section 5 RALLY_WON_RIGHT.
    fun rallyWonRight() = applyRally(Side.RIGHT)

    private fun applyRally(winningSide: Side) {
        if (state.gameEnded) return

        snapshot()

        if (winningSide == state.servingSide) {
            // Case A: serving side won their own rally.
            state = if (winningSide == Side.LEFT) {
                state.copy(leftScore = state.leftScore + 1)
            } else {
                state.copy(rightScore = state.rightScore + 1)
            }
        } else if (state.gameMode == GameMode.SINGLES) {
            // Spec 01 Section 9a: SINGLES has no B1 step — a single fault is
            // always a full side-out, regardless of the current serverNumber.
            state = fullSideOut(winningSide)
        } else {
            // Case B: non-serving side won — fault/side-out (DOUBLES).
            state = if (state.serverNumber == 1) {
                // B1: move to second server on the same side.
                state.copy(serverNumber = 2)
            } else {
                // B2: full side-out to the winning side.
                fullSideOut(winningSide)
            }
        }
    }

    // Full side-out: serve passes to the winning side, starting at 0-0-2
    // (Spec 01 Section 5 Case B2, Section 9a SINGLES fault).
    private fun fullSideOut(winningSide: Side): ScoreboardState =
        state.copy(servingSide = winningSide, serverNumber = 2)

    // Spec 01 Section 5 SWITCH_COURTS.
    fun switchCourts() {
        if (state.gameEnded) return

        snapshot()

        state = state.copy(
            leftScore = state.rightScore,
            rightScore = state.leftScore,
            servingSide = otherSide(state.servingSide)
        )
    }

    // Spec 01 Section 5a UNDO (amended: applies uniformly, including
    // immediately after END_GAME and RESET).
    fun undo() {
        previousState?.let {
            state = it
            previousState = null
        }
    }

    // Spec 01 Section 5b END_GAME (amended: snapshot before freezing).
    fun endGame() {
        if (state.gameEnded) return

        snapshot()
        state = state.copy(gameEnded = true)
    }

    // Spec 01 Section 5c RESET (amended: snapshot before re-initializing).
    fun reset(startingSide: Side, mode: GameMode = GameMode.DOUBLES) {
        snapshot()
        init(startingSide, mode)
    }

    private fun snapshot() {
        previousState = state
    }

    private fun otherSide(side: Side): Side =
        if (side == Side.LEFT) Side.RIGHT else Side.LEFT
}
