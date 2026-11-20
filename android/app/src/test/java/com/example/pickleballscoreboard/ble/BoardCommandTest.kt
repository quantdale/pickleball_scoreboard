package com.example.pickleballscoreboard.ble

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test

// Spec 02 Section 4: the exact command bytes the firmware accepts.
class BoardCommandTest {
    @Test
    fun wireBytesMatchSpec() {
        val expected =
            mapOf(
                BoardCommand.LeftPlus to "+L",
                BoardCommand.LeftMinus to "-L",
                BoardCommand.RightPlus to "+R",
                BoardCommand.RightMinus to "-R",
                BoardCommand.NextServe to "S",
                BoardCommand.SwitchCourts to "C",
                BoardCommand.EndGame to "E",
                BoardCommand.Undo to "U",
                BoardCommand.NewDoublesGame to "0D",
                BoardCommand.NewSinglesGame to "0S",
            )
        assertEquals(BoardCommand.values().toSet(), expected.keys)
        for ((command, wire) in expected) assertArrayEquals(wire.toByteArray(Charsets.US_ASCII), command.bytes)
    }

    @Test
    fun noCommandReusesAPortraitScoringOrResetWrite() {
        val portrait = setOf("L", "R", "0LD", "0LS", "0RD", "0RS")
        for (command in BoardCommand.values()) assertFalse(command.wire, command.wire in portrait)
    }

    @Test
    fun eachCallReturnsAnIndependentBuffer() {
        val first = BoardCommand.LeftPlus.bytes
        first[0] = 0
        assertArrayEquals(byteArrayOf('+'.code.toByte(), 'L'.code.toByte()), BoardCommand.LeftPlus.bytes)
    }
}
