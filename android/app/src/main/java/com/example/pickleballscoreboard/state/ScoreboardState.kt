package com.example.pickleballscoreboard.state

// Full game state per Spec 01 Section 2.
// gameMode is structurally reserved (default DOUBLES) and not used by the v1 logic.
data class ScoreboardState(
    val leftScore: Int = 0,
    val rightScore: Int = 0,
    val servingSide: Side = Side.LEFT,
    val serverNumber: Int = 2,
    val gameMode: GameMode = GameMode.DOUBLES,
    val gameEnded: Boolean = false
)

enum class Side {
    LEFT,
    RIGHT
}

enum class GameMode {
    SINGLES,
    DOUBLES
}
