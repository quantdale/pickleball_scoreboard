package com.example.pickleballscoreboard.state

import com.example.pickleballscoreboard.ble.BoardCommand

// Test oracle only: an independent Kotlin reading of Spec 01. The app never runs it;
// production state always comes from the board (Spec 02 §2).
class ScoreboardStateMachine(
    private val capacity: Int = 2048,
) {
    var state = ScoreboardState()
        private set
    private val history = ArrayDeque<ScoreboardState>()

    fun apply(command: BoardCommand) {
        val live = !state.gameEnded
        val next =
            when (command) {
                BoardCommand.LeftPlus -> state.copy(leftScore = state.leftScore + 1).takeIf { live && state.leftScore < 99 }
                BoardCommand.LeftMinus -> state.copy(leftScore = state.leftScore - 1).takeIf { live && state.leftScore > 0 }
                BoardCommand.RightPlus -> state.copy(rightScore = state.rightScore + 1).takeIf { live && state.rightScore < 99 }
                BoardCommand.RightMinus -> state.copy(rightScore = state.rightScore - 1).takeIf { live && state.rightScore > 0 }
                BoardCommand.NextServe -> nextServe().takeIf { live }
                BoardCommand.SwitchCourts ->
                    state
                        .copy(leftScore = state.rightScore, rightScore = state.leftScore, servingSide = state.servingSide.other())
                        .takeIf { live }
                BoardCommand.EndGame -> state.copy(gameEnded = true).takeIf { live }
                BoardCommand.NewDoublesGame -> ScoreboardState()
                BoardCommand.NewSinglesGame -> ScoreboardState(gameMode = GameMode.SINGLES)
                BoardCommand.Undo -> {
                    history.removeLastOrNull()?.let { state = it.copy(canUndo = history.isNotEmpty()) }
                    return
                }
            } ?: return
        history.addLast(state.copy(canUndo = false))
        if (history.size > capacity) history.removeFirst()
        state = next.copy(canUndo = true)
    }

    // Spec 01 §5: Doubles L1 -> L2 -> R1 -> R2; Singles alternates sides at server 1.
    private fun nextServe(): ScoreboardState =
        if (state.gameMode == GameMode.DOUBLES && state.serverNumber == 1) {
            state.copy(serverNumber = 2)
        } else {
            state.copy(serverNumber = 1, servingSide = state.servingSide.other())
        }

    private fun Side.other() = if (this == Side.LEFT) Side.RIGHT else Side.LEFT
}
