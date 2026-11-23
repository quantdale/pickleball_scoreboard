package com.example.pickleballscoreboard.ble

import com.example.pickleballscoreboard.state.GameMode
import com.example.pickleballscoreboard.state.ScoreboardState
import com.example.pickleballscoreboard.state.Side
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

// Spec 02 Section 5: exact Manual State payloads only.
class StatePayloadParserTest {
    @Test
    fun parsesEveryField() {
        assertEquals(
            ScoreboardState(7, 5, Side.RIGHT, 2, GameMode.DOUBLES, gameEnded = false, canUndo = true),
            parseStatePayload("7,5,R,2,0,D,1"),
        )
        assertEquals(ScoreboardState(), parseStatePayload("0,0,L,1,0,D,0"))
        assertEquals(
            ScoreboardState(99, 99, Side.LEFT, 1, GameMode.SINGLES, gameEnded = true, canUndo = true),
            parseStatePayload("99,99,L,1,1,S,1"),
        )
    }

    @Test
    fun rejectsLegacyAndPortraitFraming() {
        assertNull(parseStatePayload("0,0,L,1,0,D")) // Legacy State has six fields
        assertNull(parseStatePayload("0,0,L,2,0,D,3")) // portrait blocked-history status
        assertNull(parseStatePayload("0,0,L,2,0,D,2"))
        assertNull(parseStatePayload("0,0,L,1,0,D,1,1"))
    }

    @Test
    fun rejectsNonCanonicalOrOutOfRangeScores() {
        for (score in listOf("", "-1", "+1", "01", "100", "1.0", " 1", "١")) {
            assertNull(score, parseStatePayload("$score,0,L,1,0,D,0"))
            assertNull(score, parseStatePayload("0,$score,L,1,0,D,0"))
        }
    }

    @Test
    fun rejectsInvalidEnumsAndSinglesServerTwo() {
        assertNull(parseStatePayload("0,0,X,1,0,D,0"))
        assertNull(parseStatePayload("0,0,L,0,0,D,0"))
        assertNull(parseStatePayload("0,0,L,3,0,D,0"))
        assertNull(parseStatePayload("0,0,L,1,2,D,0"))
        assertNull(parseStatePayload("0,0,L,1,0,X,0"))
        assertNull(parseStatePayload("0,0,L,1,0,D,x"))
        assertNull(parseStatePayload("0,0,l,1,0,d,0"))
        assertNull(parseStatePayload("0,0,L,2,0,S,1")) // Spec 01 §2: Singles is always server 1
    }

    @Test
    fun everyValidPositionRoundTrips() {
        for (left in 0..99) {
            for (side in Side.values()) {
                for ((mode, server) in listOf(GameMode.DOUBLES to 1, GameMode.DOUBLES to 2, GameMode.SINGLES to 1)) {
                    val payload = "$left,${99 - left},${side.name.first()},$server,${left % 2},${mode.name.first()},${left % 3 % 2}"
                    val state = checkNotNull(parseStatePayload(payload))
                    assertEquals(payload, state.run {
                        "$leftScore,$rightScore,${servingSide.name.first()},$serverNumber,${if (gameEnded) 1 else 0}," +
                            "${gameMode.name.first()},${if (canUndo) 1 else 0}"
                    })
                }
            }
        }
    }
}
