package com.example.pickleballscoreboard.state

import com.example.pickleballscoreboard.ble.parseStatePayload
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
        ended: Boolean,
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
        ended: Boolean,
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
        assertState(sm.state, 1, 0, Side.RIGHT, 1, false)
        sm.rallyWonLeft()
        assertState(sm.state, 1, 0, Side.RIGHT, 2, false)
        sm.rallyWonLeft()
        assertState(sm.state, 1, 0, Side.LEFT, 1, false)
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
        val sm = ScoreboardStateMachine()
        sm.rallyWonRight()
        sm.rallyWonLeft()
        assertState(sm.state, 0, 0, Side.RIGHT, 2, false)
    }

    @Test
    fun caseB2_fullSideOut() {
        val sm = ScoreboardStateMachine()
        sm.init(Side.LEFT)
        sm.rallyWonRight()
        // Spec 01 Section 5 Case B2: no score changes on a side-out.
        assertState(sm.state, 0, 0, Side.RIGHT, 1, false)
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
    fun fullHistoryRestoresEveryPositionAcrossModeChanges() {
        for (mode in GameMode.values()) {
            val sm = ScoreboardStateMachine()
            sm.init(Side.RIGHT, mode)
            val timeline = mutableListOf<Pair<ScoreboardState, Boolean>>()
            repeat(600) { i ->
                for (action in listOf(
                    sm::rallyWonLeft,
                    sm::rallyWonRight,
                    sm::rallyWonRight,
                    sm::switchCourts,
                    sm::rallyWonLeft,
                    sm::endGame,
                    { if (i % 2 == 0) sm.reset(Side.RIGHT) else sm.reset(Side.LEFT, GameMode.SINGLES) },
                )) {
                    timeline.add(sm.state to sm.openingService)
                    action()
                }
            }
            for ((position, opening) in timeline.asReversed()) {
                assertTrue(sm.canUndo)
                sm.undo()
                assertEquals(position, sm.state)
                assertEquals(opening, sm.openingService)
            }
            assertFalse(sm.canUndo)
            sm.undo()
            assertStateWithMode(sm.state, 0, 0, Side.RIGHT, 2, mode, false)
        }
    }

    @Test
    fun undoAfterEndGame() {
        val sm = ScoreboardStateMachine()
        sm.init(Side.LEFT)
        sm.rallyWonLeft()
        sm.rallyWonRight()
        sm.endGame()
        assertState(sm.state, 1, 0, Side.RIGHT, 1, true)

        sm.undo()
        assertState(sm.state, 1, 0, Side.RIGHT, 1, false)
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
    fun singlesAlternatesWithOneFaultPerSide() {
        val sm = ScoreboardStateMachine()
        sm.init(Side.LEFT, GameMode.SINGLES)
        repeat(4) {
            sm.rallyWonRight()
            assertStateWithMode(sm.state, 0, 0, Side.RIGHT, 2, GameMode.SINGLES, false)
            sm.rallyWonLeft()
            assertStateWithMode(sm.state, 0, 0, Side.LEFT, 2, GameMode.SINGLES, false)
            assertFalse(sm.openingService)
        }
    }

    @Test
    fun sharedServingRotationMatchesFirmwareWireProjection() {
        val sm = ScoreboardStateMachine()
        val fixture = checkNotNull(javaClass.getResourceAsStream("/serving-rotation.inc"))
        val row = Regex("""STEP\("([A-Z0]+)",\s*"([^"]+)", ([01]), ([01])\)""")
        fixture.bufferedReader().useLines { lines ->
            lines.filter { it.startsWith("STEP(") }.forEachIndexed { index, line ->
                val (command, expected, opening, changed) = checkNotNull(row.matchEntire(line)).destructured
                val before = sm.state
                when (command) {
                    "L" -> {
                        sm.rallyWonLeft()
                    }

                    "R" -> {
                        sm.rallyWonRight()
                    }

                    "C" -> {
                        sm.switchCourts()
                    }

                    "U" -> {
                        sm.undo()
                    }

                    "E" -> {
                        sm.endGame()
                    }

                    else -> {
                        sm.reset(
                            if (command[1] == 'L') Side.LEFT else Side.RIGHT,
                            if (command[2] == 'D') GameMode.DOUBLES else GameMode.SINGLES,
                        )
                    }
                }
                assertEquals("step $index", parseStatePayload(expected), sm.state)
                assertEquals("opening at $index", opening == "1", sm.openingService)
                assertEquals("change at $index", changed == "1", before != sm.state)
            }
        }
    }

    @Test
    fun initDiscardsOldUndoHistory() {
        val sm = ScoreboardStateMachine()
        sm.rallyWonLeft()
        sm.init(Side.RIGHT)
        sm.undo()
        assertState(sm.state, 0, 0, Side.RIGHT, 2, false)
        assertTrue(sm.openingService)
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
