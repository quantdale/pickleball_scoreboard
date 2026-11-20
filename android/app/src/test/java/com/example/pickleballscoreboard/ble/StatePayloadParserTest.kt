package com.example.pickleballscoreboard.ble

import com.example.pickleballscoreboard.state.GameMode
import com.example.pickleballscoreboard.state.ScoreboardState
import com.example.pickleballscoreboard.state.Side
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

// Unit tests for the BLE Notify payload parser.
// See docs/specs/02-ble-protocol.md Section 5 for the authoritative format.
class StatePayloadParserTest {
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
        ended: Boolean,
        mode: GameMode,
    ) {
        assertState(state, left, right, side, server, ended)
        assertEquals(mode, state.gameMode)
    }

    @Test
    fun parseBasicPayloadLeftServing() {
        val state = parseStatePayload("3,5,L,2,0,D")
        assertStateWithMode(state!!, 3, 5, Side.LEFT, 2, false, GameMode.DOUBLES)
    }

    @Test
    fun parseBasicPayloadRightServing() {
        val state = parseStatePayload("3,5,R,2,0,D")
        assertStateWithMode(state!!, 3, 5, Side.RIGHT, 2, false, GameMode.DOUBLES)
    }

    @Test
    fun parseServerNumberOne() {
        val state = parseStatePayload("0,0,L,1,0,D")
        assertStateWithMode(state!!, 0, 0, Side.LEFT, 1, false, GameMode.DOUBLES)
    }

    @Test
    fun parseGameEnded() {
        val state = parseStatePayload("11,9,R,2,1,D")
        assertStateWithMode(state!!, 11, 9, Side.RIGHT, 2, true, GameMode.DOUBLES)
    }

    @Test
    fun parseMultiDigitScores() {
        val state = parseStatePayload("127,134,R,2,0,D")
        assertStateWithMode(state!!, 127, 134, Side.RIGHT, 2, false, GameMode.DOUBLES)
    }

    @Test
    fun parseSinglesMode() {
        val state = parseStatePayload("3,5,L,2,0,S")
        assertStateWithMode(state!!, 3, 5, Side.LEFT, 2, false, GameMode.SINGLES)
    }

    @Test
    fun emptyStringIsMalformed() {
        assertNull(parseStatePayload(""))
    }

    @Test
    fun tooFewFieldsIsMalformed() {
        assertNull(parseStatePayload("3,R,2,0,D"))
    }

    @Test
    fun oldFiveFieldPayloadIsNowMalformed() {
        // Spec 02 Section 5: the pre-SINGLES five-field payload is now
        // malformed input (missing the gameMode field) and must be rejected,
        // not parsed with an implied default mode.
        assertNull(parseStatePayload("3,5,R,2,0"))
    }

    @Test
    fun tooManyFieldsIsMalformed() {
        assertNull(parseStatePayload("3,5,R,2,0,D,extra"))
    }

    @Test
    fun nonNumericLeftScoreIsMalformed() {
        assertNull(parseStatePayload("abc,5,R,2,0,D"))
    }

    @Test
    fun nonNumericRightScoreIsMalformed() {
        assertNull(parseStatePayload("3,xyz,R,2,0,D"))
    }

    @Test
    fun invalidServingSideIsMalformed() {
        assertNull(parseStatePayload("3,5,X,2,0,D"))
    }

    @Test
    fun invalidServerNumberIsMalformed() {
        assertNull(parseStatePayload("3,5,R,3,0,D"))
    }

    @Test
    fun invalidGameEndedFlagIsMalformed() {
        assertNull(parseStatePayload("3,5,R,2,2,D"))
    }

    @Test
    fun invalidGameModeFlagIsMalformed() {
        assertNull(parseStatePayload("3,5,R,2,0,X"))
    }

    @Test
    fun nonCanonicalNumbersAreMalformed() {
        // Spec 02 Sections 5/7: scores are canonical nonnegative ASCII decimals.
        for (score in listOf("-1", "+1", "01", " 1", "2147483648")) {
            assertNull(parseStatePayload("$score,5,R,2,0,D"))
            assertNull(parseStatePayload("5,$score,R,2,0,D"))
        }
        assertNull(parseStatePayload("3,5,R,02,0,D"))
    }

    @Test
    fun extendedStreamReportsUndoAndMemoryPressure() {
        for (flags in 0..3) {
            val state = checkNotNull(parseStatePayload("4,4,L,2,0,S,$flags", extended = true))
            assertStateWithMode(state, 4, 4, Side.LEFT, 2, false, GameMode.SINGLES)
            assertEquals(flags and 1 != 0, state.canUndo)
            assertEquals(flags and 2 != 0, state.historyBlocked)
        }
        assertNull(parseStatePayload("4,4,L,2,0,S")?.canUndo)
    }

    @Test
    fun wrongStreamOrNonCanonicalUndoStatusIsRejected() {
        assertNull(parseStatePayload("4,4,L,2,0,S,0"))
        assertNull(parseStatePayload("4,4,L,2,0,S", extended = true))
        for (bad in listOf("", "4", "01", " 1", "-1", "１", "1,0")) {
            assertNull(parseStatePayload("4,4,L,2,0,S,$bad", extended = true))
        }
    }

    @Test
    fun defaultStatePayload() {
        val state = parseStatePayload("0,0,L,2,0,D")
        assertState(state!!, 0, 0, Side.LEFT, 2, false)
        assertEquals(GameMode.DOUBLES, state.gameMode)
    }
}
