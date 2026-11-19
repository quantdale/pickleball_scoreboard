package com.example.pickleballscoreboard.state

// Scoring state machine implementation.
// See docs/specs/01-scoring-state-machine.md for the authoritative rules.
//
// Test-only mirror. The Android controller never applies scoring transitions.
class ScoreboardStateMachine {
    var state: ScoreboardState = ScoreboardState()
        private set

    var openingService: Boolean = true
        private set

    private data class Snapshot(val state: ScoreboardState, val openingService: Boolean)
    private var previous: Snapshot? = null

    // Spec 01 Sections 4 and 9b: initialization is not an undoable Reset.
    fun init(startingSide: Side, mode: GameMode = GameMode.DOUBLES) {
        start(startingSide, mode)
        previous = null
    }

    private fun start(startingSide: Side, mode: GameMode) {
        state = ScoreboardState(servingSide = startingSide, gameMode = mode)
        openingService = mode == GameMode.DOUBLES
    }

    // Spec 01 Section 5 RALLY_WON_LEFT.
    fun rallyWonLeft() = applyRally(Side.LEFT)

    // Spec 01 Section 5 RALLY_WON_RIGHT.
    fun rallyWonRight() = applyRally(Side.RIGHT)

    private fun applyRally(winningSide: Side) {
        if (state.gameEnded) return

        if (winningSide == state.servingSide) {
            val score = if (winningSide == Side.LEFT) state.leftScore else state.rightScore
            if (score == Int.MAX_VALUE) return
            snapshot()
            state = if (winningSide == Side.LEFT) {
                state.copy(leftScore = score + 1)
            } else {
                state.copy(rightScore = score + 1)
            }
            return
        }

        snapshot()
        val doubles = state.gameMode == GameMode.DOUBLES
        if (doubles && !openingService && state.serverNumber == 1) {
            state = state.copy(serverNumber = 2)
        } else {
            // Spec 01 §5 B2 and §9a: side-out ends the opening sequence.
            openingService = false
            state = state.copy(servingSide = winningSide, serverNumber = if (doubles) 1 else 2)
        }
    }

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
        previous?.let {
            state = it.state
            openingService = it.openingService
            previous = null
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
        start(startingSide, mode)
    }

    private fun snapshot() {
        previous = Snapshot(state, openingService)
    }

    private fun otherSide(side: Side): Side =
        if (side == Side.LEFT) Side.RIGHT else Side.LEFT
}
