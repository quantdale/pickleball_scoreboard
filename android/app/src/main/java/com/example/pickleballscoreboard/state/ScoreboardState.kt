package com.example.pickleballscoreboard.state

// Full game state per Spec 01 Section 2.
// gameMode selects the SINGLES/DOUBLES scoring rules (Spec 01 Section 9a); see
// ScoreboardStateMachine.
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
