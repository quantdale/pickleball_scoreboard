package com.example.pickleballscoreboard.state

import com.example.pickleballscoreboard.ble.BoardCommand
import com.example.pickleballscoreboard.ble.parseStatePayload
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

// Spec 01: the Kotlin oracle and the firmware consume the same shared fixture.
class ScoreboardStateMachineTest {
    private fun legacyWire(state: ScoreboardState) =
        with(state) {
            "$leftScore,$rightScore,${servingSide.name.first()},$serverNumber,${if (gameEnded) 1 else 0},${gameMode.name.first()}"
        }

    @Test
    fun sharedFixtureMatchesFirmwareWireProjection() = assertEquals(50, replay("/manual-controller.inc"))

    @Test
    fun firmwareRecordedRandomWalkMatchesOracle() = assertEquals(600, replay("/manual-random-walk.inc"))

    private fun replay(resource: String): Int {
        val sm = ScoreboardStateMachine()
        val fixture = checkNotNull(javaClass.getResourceAsStream(resource))
        val row = Regex("""STEP\("([^"]+)",\s*"([^"]+)", ([01]), ([01])\)""")
        var steps = 0
        fixture.bufferedReader().useLines { lines ->
            lines.filter { it.startsWith("STEP(") }.forEach { line ->
                val (wire, expected, undo, changed) = checkNotNull(row.matchEntire(line.trim())).destructured
                val before = legacyWire(sm.state)
                BoardCommand.values().firstOrNull { it.wire == wire }?.let(sm::apply)
                assertEquals(line, expected, legacyWire(sm.state))
                assertEquals(line, undo == "1", sm.state.canUndo)
                assertEquals(line, changed == "1", before != expected)
                // The Android parser accepts exactly what the board would notify.
                assertEquals(line, sm.state, parseStatePayload("$expected,$undo"))
                steps++
            }
        }
        return steps
    }

    @Test
    fun workedExampleFromSpec() {
        val sm = ScoreboardStateMachine()
        repeat(7) { sm.apply(BoardCommand.LeftPlus) }
        repeat(5) { sm.apply(BoardCommand.RightPlus) }
        sm.apply(BoardCommand.NextServe)
        assertEquals("7,5,L,2,0,D", legacyWire(sm.state))
        sm.apply(BoardCommand.NextServe)
        sm.apply(BoardCommand.RightMinus)
        sm.apply(BoardCommand.SwitchCourts)
        assertEquals("4,7,L,1,0,D", legacyWire(sm.state))
        sm.apply(BoardCommand.EndGame)
        sm.apply(BoardCommand.LeftPlus)
        assertEquals("4,7,L,1,1,D", legacyWire(sm.state))
        sm.apply(BoardCommand.Undo)
        assertEquals("4,7,L,1,0,D", legacyWire(sm.state))
        sm.apply(BoardCommand.NewSinglesGame)
        sm.apply(BoardCommand.NextServe)
        assertEquals("0,0,R,1,0,S", legacyWire(sm.state))
        assertTrue(sm.state.canUndo)
    }

    @Test
    fun serveCycleHasExactlyFourDoublesStates() {
        val sm = ScoreboardStateMachine()
        val seen =
            (0 until 8).map {
                sm.apply(BoardCommand.NextServe)
                sm.state.servingSide to sm.state.serverNumber
            }
        assertEquals(
            listOf(Side.LEFT to 2, Side.RIGHT to 1, Side.RIGHT to 2, Side.LEFT to 1).let { it + it },
            seen,
        )
    }

    @Test
    fun fullHistoryDropsOldestAndUnwindsTheRest() {
        val sm = ScoreboardStateMachine(capacity = 32) // Spec 01 §6
        val timeline = mutableListOf<ScoreboardState>()
        repeat(300) { i ->
            timeline.add(sm.state)
            sm.apply(if (i % 3 == 0) BoardCommand.NextServe else if (sm.state.leftScore < 99) BoardCommand.LeftPlus else BoardCommand.SwitchCourts)
        }
        repeat(32) { i ->
            sm.apply(BoardCommand.Undo)
            assertEquals(timeline[timeline.size - 1 - i].copy(canUndo = false), sm.state.copy(canUndo = false))
        }
        assertFalse(sm.state.canUndo)
        val stuck = sm.state
        sm.apply(BoardCommand.Undo)
        assertEquals(stuck, sm.state)
    }
}
