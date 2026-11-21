package com.example.pickleballscoreboard.ble

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

// Spec 02 §§4/5a, Spec 03 §3: palette, wire commands and exact color reports.
class BoardColorTest {
    @Test
    fun paletteMatchesTheFirmwareInOrderWithWhiteFirst() {
        assertEquals("WRYGCB", LedColor.entries.joinToString("") { it.code.toString() })
        assertEquals(listOf("KW", "KR", "KY", "KG", "KC", "KB"), LedColor.entries.map { it.wire })
        for (color in LedColor.entries) {
            // Whole channels only, as the board drives them.
            for (shift in listOf(16, 8, 0)) {
                val channel = (color.argb shr shift) and 0xFF
                assertTrue(color.label, channel == 0L || channel == 0xFFL)
            }
        }
    }

    @Test
    fun reportsParseExactly() {
        for (color in LedColor.entries) assertEquals(color, parseColorPayload("K,${color.code}"))
        for (payload in listOf("K,X", "K,r", "K,", "KR", "K,RR", " K,R", "B,1,4", "0,0,L,2,0,D,0", "K;R", "")) {
            assertNull(payload, parseColorPayload(payload))
        }
    }

    @Test
    fun noReportIsMistakenForAnother() {
        for (color in LedColor.entries) {
            assertNull(parseBrightnessPayload("K,${color.code}"))
            assertNull(parseStatePayload("K,${color.code}"))
        }
        assertNull(parseColorPayload("B,2,4"))
    }

    @Test
    fun colorWritesNeverCollideWithScoringOrBrightness() {
        val other = BoardCommand.entries.map { it.wire }.toSet() + (1..9).map(::brightnessWire)
        for (color in LedColor.entries) assertTrue(color.wire, color.wire !in other)
    }
}
