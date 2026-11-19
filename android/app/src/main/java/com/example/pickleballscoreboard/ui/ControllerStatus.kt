package com.example.pickleballscoreboard.ui

// Spec 02 Section 6: ready is emitted only by the subscribed GATT session.
internal enum class ConnectionPhase(
    val label: String,
) {
    Disconnected("Disconnected"),
    Scanning("Scanning"),
    Connecting("Connecting"),
    Synchronizing("Synchronizing"),
    Ready("Connected / Ready"),
    ReconnectRequired("Reconnect required");

    val hasLinkActions: Boolean get() = this == Connecting || this == Synchronizing || this == Ready
}

internal data class ControllerStatus(
    val phase: ConnectionPhase = ConnectionPhase.Disconnected,
    val detail: String? = null,
    val pendingWrites: Int = 0,
    val submitted: Boolean = false,
) {
    val ready: Boolean get() = phase == ConnectionPhase.Ready
    val transportText: String get() =
        when {
            pendingWrites == 1 -> "1 command queued or sending"
            pendingWrites > 1 -> "$pendingWrites commands queued or sending"
            submitted -> "Write acknowledged · score updates separately"
            else -> "Score received from board"
        }
}
