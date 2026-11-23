package com.example.pickleballscoreboard.ble

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

// Spec 02 Sections 4–6: brightness reports, writes and coalesced stepping.
class BoardBrightnessTest {
    @Test
    fun parsesExactReports() {
        assertEquals(BoardBrightness(1, 4), parseBrightnessPayload("B,1,4"))
        assertEquals(BoardBrightness(4, 4), parseBrightnessPayload("B,4,4"))
        assertEquals(BoardBrightness(3, 8), parseBrightnessPayload("B,3,8"))
    }

    @Test
    fun rejectsMalformedReports() {
        for (payload in listOf("", "B", "B,1", "B,0,4", "B,5,4", "B,1,0", "B,1,4,1", "b,1,4", "B,01,4", "B,1,10", " B,1,4", "B,1,4\n", "B;1;4", "B,١,4")) {
            assertNull(payload, parseBrightnessPayload(payload))
        }
    }

    @Test
    fun stateAndBrightnessReportsNeverParseAsEachOther() {
        // Android 2.0.0 runs the same state parser and therefore ignores every report.
        for (level in 1..9) assertNull(parseStatePayload("B,$level,9"))
        for (payload in listOf("0,0,L,1,0,D,0", "99,99,R,2,1,D,1")) assertNull(parseBrightnessPayload(payload))
    }

    @Test
    fun writesAreTwoAsciiBytesThatNeverCollideWithScoring() {
        for (level in 1..9) {
            val wire = brightnessWire(level)
            assertEquals("B$level", wire)
            assert(BoardCommand.values().none { it.wire == wire })
        }
        for (invalid in listOf(0, 10, -1)) {
            try {
                brightnessWire(invalid)
                throw AssertionError("accepted $invalid")
            } catch (_: IllegalArgumentException) {
            }
        }
    }

    @Test
    fun stepsStayWithinTheReportedRange() {
        assertNull(stepBrightness(BoardBrightness(1, 4), null, -1))
        assertEquals(2, stepBrightness(BoardBrightness(1, 4), null, 1))
        assertEquals(3, stepBrightness(BoardBrightness(4, 4), null, -1))
        assertNull(stepBrightness(BoardBrightness(4, 4), null, 1))
        assertNull(stepBrightness(BoardBrightness(1, 1), null, 1))
    }

    @Test
    fun rapidTapsAccumulateFromThePendingRequest() {
        val reported = BoardBrightness(1, 4)
        var pending: Int? = null
        repeat(5) { stepBrightness(reported, pending, 1)?.let { pending = it } }
        assertEquals(4, pending) // capped at the board's maximum, not 6
        assertEquals(3, stepBrightness(reported, pending, -1))
        assertNull(stepBrightness(BoardBrightness(4, 4), 4, 1))
    }
}
