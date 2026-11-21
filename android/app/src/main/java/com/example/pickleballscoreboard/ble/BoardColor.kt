package com.example.pickleballscoreboard.ble

// Spec 03 §3: the board's palette. Every lit LED uses one of these; each turns whole
// RGB channels fully on, so the preview color is the same full-scale value.
enum class LedColor(
    val code: Char,
    val label: String,
    val argb: Long,
) {
    White('W', "White", 0xFFFFFFFF),
    Red('R', "Red", 0xFFFF0000),
    Yellow('Y', "Yellow", 0xFFFFFF00),
    Green('G', "Green", 0xFF00FF00),
    Cyan('C', "Cyan", 0xFF00FFFF),
    Blue('B', "Blue", 0xFF0000FF),
    ;

    // Spec 02 §4: "K" and the color's letter.
    val wire: String get() = "K$code"
}

// Spec 02 §§5a/7: an exact color report on the Manual State stream; anything else is ignored.
fun parseColorPayload(payload: String): LedColor? {
    if (payload.length != 3 || !payload.startsWith("K,")) return null
    return LedColor.entries.firstOrNull { it.code == payload[2] }
}
