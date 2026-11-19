package com.example.pickleballscoreboard.ble

import com.example.pickleballscoreboard.state.GameMode
import com.example.pickleballscoreboard.state.ScoreboardState
import com.example.pickleballscoreboard.state.Side

private val canonicalScore = Regex("0|[1-9][0-9]*")

// Spec 02 Sections 5/7: exactly six canonical ASCII fields, malformed input ignored.
fun parseStatePayload(payload: String): ScoreboardState? {
    val parts = payload.split(',')
    if (parts.size != 6) return null
    if (!parts[0].matches(canonicalScore) || !parts[1].matches(canonicalScore)) return null
    val left = parts[0].toIntOrNull() ?: return null
    val right = parts[1].toIntOrNull() ?: return null
    val side =
        when (parts[2]) {
            "L" -> Side.LEFT
            "R" -> Side.RIGHT
            else -> return null
        }
    val server =
        when (parts[3]) {
            "1" -> 1
            "2" -> 2
            else -> return null
        }
    val ended =
        when (parts[4]) {
            "0" -> false
            "1" -> true
            else -> return null
        }
    val mode =
        when (parts[5]) {
            "D" -> GameMode.DOUBLES
            "S" -> GameMode.SINGLES
            else -> return null
        }
    return ScoreboardState(
        leftScore = left,
        rightScore = right,
        servingSide = side,
        serverNumber = server,
        gameEnded = ended,
        gameMode = mode,
    )
}
