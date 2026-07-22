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
        ended: Boolean
    ) {
        assertEquals(left, state.leftScore)
        assertEquals(right, state.rightScore)
        assertEquals(side, state.servingSide)
        assertEquals(server, state.serverNumber)
        assertEquals(ended, state.gameEnded)
    }

    @Test
    fun parseBasicPayloadLeftServing() {
        val state = parseStatePayload("3,5,L,2,0")
        assertState(state!!, 3, 5, Side.LEFT, 2, false)
    }

    @Test
    fun parseBasicPayloadRightServing() {
        val state = parseStatePayload("3,5,R,2,0")
        assertState(state!!, 3, 5, Side.RIGHT, 2, false)
    }

    @Test
    fun parseServerNumberOne() {
        val state = parseStatePayload("0,0,L,1,0")
        assertState(state!!, 0, 0, Side.LEFT, 1, false)
    }

    @Test
    fun parseGameEnded() {
        val state = parseStatePayload("11,9,R,2,1")
        assertState(state!!, 11, 9, Side.RIGHT, 2, true)
    }

    @Test
    fun parseMultiDigitScores() {
        val state = parseStatePayload("127,134,R,2,0")
        assertState(state!!, 127, 134, Side.RIGHT, 2, false)
    }

    @Test
    fun emptyStringIsMalformed() {
        assertNull(parseStatePayload(""))
    }

    @Test
    fun tooFewFieldsIsMalformed() {
        assertNull(parseStatePayload("3,R,2,0"))
    }

    @Test
    fun tooManyFieldsIsMalformed() {
        assertNull(parseStatePayload("3,5,R,2,0,extra"))
    }

    @Test
    fun nonNumericLeftScoreIsMalformed() {
        assertNull(parseStatePayload("abc,5,R,2,0"))
    }

    @Test
    fun nonNumericRightScoreIsMalformed() {
        assertNull(parseStatePayload("3,xyz,R,2,0"))
    }

    @Test
    fun invalidServingSideIsMalformed() {
        assertNull(parseStatePayload("3,5,X,2,0"))
    }

    @Test
    fun invalidServerNumberIsMalformed() {
        assertNull(parseStatePayload("3,5,R,3,0"))
    }

    @Test
    fun invalidGameEndedFlagIsMalformed() {
        assertNull(parseStatePayload("3,5,R,2,2"))
    }

    @Test
    fun negativeScoreIsParsedButLeftAsInvalidByStateMachine() {
        // The parser itself accepts negative integers because toIntOrNull does.
        // The spec does not define negative scores, but the parser is lenient here
        // and the authoritative state machine never produces them.
        val state = parseStatePayload("-1,5,R,2,0")
        assertEquals(-1, state!!.leftScore)
    }

    @Test
    fun defaultStatePayload() {
        val state = parseStatePayload("0,0,L,2,0")
        assertState(state!!, 0, 0, Side.LEFT, 2, false)
        assertEquals(GameMode.DOUBLES, state.gameMode)
    }
}
