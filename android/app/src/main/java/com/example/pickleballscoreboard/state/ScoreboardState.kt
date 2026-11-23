package com.example.pickleballscoreboard.state

// Spec 01 Section 2: the board's authoritative position, as reported on Manual State.
data class ScoreboardState(
    val leftScore: Int = 0,
    val rightScore: Int = 0,
    val servingSide: Side = Side.LEFT,
    val serverNumber: Int = 1,
    val gameMode: GameMode = GameMode.DOUBLES,
    val gameEnded: Boolean = false,
    val canUndo: Boolean = false,
)

enum class Side {
    LEFT,
    RIGHT,
}

enum class GameMode {
    SINGLES,
    DOUBLES,
}
