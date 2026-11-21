package com.example.pickleballscoreboard.ble

// Spec 02 §5: the board's active brightness level and its build's maximum.
data class BoardBrightness(
    val level: Int,
    val maxLevel: Int,
)

private val brightnessReport = Regex("B,([1-9]),([1-9])")

// Spec 02 §§5/7: exact brightness reports on the Manual State stream; anything else is ignored.
fun parseBrightnessPayload(payload: String): BoardBrightness? {
    val match = brightnessReport.matchEntire(payload) ?: return null
    val level = match.groupValues[1].toInt()
    val maxLevel = match.groupValues[2].toInt()
    return if (level <= maxLevel) BoardBrightness(level, maxLevel) else null
}

// Spec 02 §6: rapid taps step from the newest unacknowledged request, so they
// accumulate without extra writes, and never leave the board's reported range.
fun stepBrightness(
    reported: BoardBrightness,
    pendingLevel: Int?,
    delta: Int,
): Int? {
    val base = pendingLevel ?: reported.level
    val next = (base + delta).coerceIn(1, reported.maxLevel)
    return next.takeIf { it != base }
}
