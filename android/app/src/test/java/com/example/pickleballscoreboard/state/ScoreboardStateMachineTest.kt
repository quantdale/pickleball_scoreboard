package com.example.pickleballscoreboard.state

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

// Unit tests for the scoring state machine.
// See docs/specs/01-scoring-state-machine.md for the authoritative rules.
class ScoreboardStateMachineTest {

    private fun assertState(
        state: ScoreboardState,
        left: Int,
        right: Int,
        side: Side,
        server: Int,
        ended: Boolean
    ) {
        assertEquals(left, state.leftScore)
        assertEquals(right, state.rightScore)
        assertEquals(side, state.servingSide)
        assertEquals(server, state.serverNumber)
        assertEquals(ended, state.gameEnded)
    }

    private fun assertStateWithMode(
        state: ScoreboardState,
        left: Int,
        right: Int,
        side: Side,
        server: Int,
        mode: GameMode,
        ended: Boolean
    ) {
        assertState(state, left, right, side, server, ended)
        assertEquals(mode, state.gameMode)
    }

    @Test
    fun initialState() {
        val sm = ScoreboardStateMachine()
        sm.init(Side.RIGHT)
        assertState(sm.state, 0, 0, Side.RIGHT, 2, false)
    }

    @Test
    fun workedExampleFromSpec() {
        val sm = ScoreboardStateMachine()
        sm.init(Side.LEFT)

        sm.rallyWonLeft()
        assertState(sm.state, 1, 0, Side.LEFT, 2, false)

        sm.rallyWonRight()
        assertState(sm.state, 1, 0, Side.RIGHT, 2, false)

        sm.rallyWonLeft()
        assertState(sm.state, 1, 0, Side.LEFT, 2, false)
    }

    @Test
    fun caseA_servingSideWins() {
        val sm = ScoreboardStateMachine()
        sm.init(Side.LEFT)
        sm.rallyWonLeft()
        assertState(sm.state, 1, 0, Side.LEFT, 2, false)
    }

    @Test
    fun caseB1_serverNumberOneFault() {
        // Use the test-only constructor to set a specific starting state.
        val sm = ScoreboardStateMachine(
            ScoreboardState(servingSide = Side.LEFT, serverNumber = 1)
        )

        sm.rallyWonRight()
        assertState(sm.state, 0, 0, Side.LEFT, 2, false)
    }

    @Test
    fun caseB2_fullSideOut() {
        val sm = ScoreboardStateMachine()
        sm.init(Side.LEFT)
        sm.rallyWonRight()
        // Spec 01 Section 5 Case B2: no score changes on a side-out.
        assertState(sm.state, 0, 0, Side.RIGHT, 2, false)
    }

    @Test
    fun switchCourts() {
        val sm = ScoreboardStateMachine()
        sm.init(Side.LEFT)
        sm.rallyWonLeft()
        sm.rallyWonLeft()
        // state is (2,0,L,2)
        sm.switchCourts()
        assertState(sm.state, 0, 2, Side.RIGHT, 2, false)
    }

    @Test
    fun undoSingleStep() {
        val sm = ScoreboardStateMachine()
        sm.init(Side.LEFT)
        sm.rallyWonLeft()
        sm.undo()
        assertState(sm.state, 0, 0, Side.LEFT, 2, false)
    }

    @Test
    fun undoDoubleDoesNothing() {
        val sm = ScoreboardStateMachine()
        sm.init(Side.LEFT)
        sm.rallyWonLeft()
        sm.undo()
        sm.undo()
        assertState(sm.state, 0, 0, Side.LEFT, 2, false)
    }

    @Test
    fun undoAfterEndGame() {
        val sm = ScoreboardStateMachine()
        sm.init(Side.LEFT)
        sm.rallyWonLeft()
        sm.rallyWonRight()
        sm.endGame()
        assertState(sm.state, 1, 0, Side.RIGHT, 2, true)

        sm.undo()
        assertState(sm.state, 1, 0, Side.RIGHT, 2, false)
    }

    @Test
    fun undoAfterReset() {
        val sm = ScoreboardStateMachine()
        sm.init(Side.LEFT)
        sm.rallyWonLeft()
        sm.rallyWonLeft()
        // state is (2,0,L,2)

        sm.reset(Side.RIGHT)
        assertState(sm.state, 0, 0, Side.RIGHT, 2, false)

        sm.undo()
        assertState(sm.state, 2, 0, Side.LEFT, 2, false)
    }

    @Test
    fun endGameFreezesRallyAndSwitchInputs() {
        val sm = ScoreboardStateMachine()
        sm.init(Side.LEFT)
        sm.rallyWonLeft()
        sm.endGame()

        // Rallies and court switches are no-ops while ended; undo is explicitly
        // allowed by the amended Spec 01 Section 5a and is tested separately.
        sm.rallyWonLeft()
        sm.rallyWonRight()
        sm.switchCourts()

        assertState(sm.state, 1, 0, Side.LEFT, 2, true)
    }

    @Test
    fun resetRecoversFromEndedState() {
        val sm = ScoreboardStateMachine()
        sm.init(Side.LEFT)
        sm.rallyWonLeft()
        sm.endGame()

        sm.reset(Side.RIGHT)
        assertState(sm.state, 0, 0, Side.RIGHT, 2, false)
    }

    // Spec 01 Section 9a: SINGLES has no B1/B2 split — a single fault is
    // always an immediate full side-out, regardless of the current
    // serverNumber.

    @Test
    fun singlesFaultIsImmediateSideOutFromServerTwo() {
        val sm = ScoreboardStateMachine()
        sm.init(Side.LEFT, GameMode.SINGLES)

        sm.rallyWonRight()
        assertStateWithMode(sm.state, 0, 0, Side.RIGHT, 2, GameMode.SINGLES, false)
    }

    @Test
    fun singlesFaultIsImmediateSideOutFromServerOne() {
        val sm = ScoreboardStateMachine(
            ScoreboardState(servingSide = Side.LEFT, serverNumber = 1, gameMode = GameMode.SINGLES)
        )

        sm.rallyWonRight()
        // Unlike DOUBLES' B1 (which would just move to serverNumber 2 on the
        // same side), SINGLES side-outs immediately regardless of serverNumber.
        assertStateWithMode(sm.state, 0, 0, Side.RIGHT, 2, GameMode.SINGLES, false)
    }

    @Test
    fun singlesCaseA_servingSideWins() {
        val sm = ScoreboardStateMachine()
        sm.init(Side.LEFT, GameMode.SINGLES)

        sm.rallyWonLeft()
        assertStateWithMode(sm.state, 1, 0, Side.LEFT, 2, GameMode.SINGLES, false)
    }

    @Test
    fun initSetsMode() {
        val sm = ScoreboardStateMachine()
        sm.init(Side.LEFT, GameMode.SINGLES)
        assertStateWithMode(sm.state, 0, 0, Side.LEFT, 2, GameMode.SINGLES, false)
    }

    @Test
    fun resetSetsMode() {
        val sm = ScoreboardStateMachine()
        sm.init(Side.LEFT) // defaults to DOUBLES
        sm.rallyWonLeft()

        sm.reset(Side.RIGHT, GameMode.SINGLES)
        assertStateWithMode(sm.state, 0, 0, Side.RIGHT, 2, GameMode.SINGLES, false)
    }
}
