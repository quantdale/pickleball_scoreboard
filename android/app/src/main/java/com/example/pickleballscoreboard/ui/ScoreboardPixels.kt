package com.example.pickleballscoreboard.ui

import com.example.pickleballscoreboard.state.ScoreboardState
import com.example.pickleballscoreboard.state.Side
import org.json.JSONObject

// Spec 03 Sections 4/6: pure pixel renderer, used by Compose AND host snapshot tests.
data class LitPixel(
    val x: Int,
    val y: Int,
)

class ScoreboardPixels(
    fontJson: String,
    arrowsJson: String,
    layoutJson: String,
) {
    private val fontObject = JSONObject(fontJson)
    private val font =
        (0..9).associate { digit ->
            digit.toString()[0] to rows(fontObject, digit.toString())
        }
    private val arrow = rows(JSONObject(arrowsJson), "ARROW_RIGHT")
    private val layout = JSONObject(layoutJson)
    val width = layout.getInt("canvasWidth")
    val height = layout.getInt("canvasHeight")

    fun pixels(state: ScoreboardState): Set<LitPixel> =
        buildSet {
            fun pixel(
                x: Int,
                y: Int,
            ) {
                if (x in 0 until width && y in 0 until height) add(LitPixel(x, y))
            }

            fun score(
                value: Int,
                center: Double,
            ) {
                val overflow = value !in 0..99
                val digits = if (overflow) "--" else value.toString()
                val scale = layout.getInt("digitScale")
                val gap = layout.getInt("digitGap")
                val top = layout.getInt("digitTopY")
                val total = digits.length * 5 * scale + (digits.length - 1) * gap
                val start = (center - total / 2.0 + 0.5).toInt()
                digits.forEachIndexed { index, digit ->
                    val x = start + index * (5 * scale + gap)
                    if (overflow) {
                        for (col in 0 until 10) for (row in 7..8) pixel(x + col, row)
                    } else {
                        font.getValue(digit).forEachIndexed { row, bits ->
                            bits.forEachIndexed { col, bit ->
                                if (bit == '1') {
                                    for (dy in 0 until scale) {
                                        for (dx in 0 until scale) {
                                            pixel(x + col * scale + dx, top + row * scale + dy)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            score(state.leftScore, layout.getDouble("leftCenterX"))
            score(state.rightScore, layout.getDouble("rightCenterX"))
            val count = if (state.serverNumber == 1) 1 else 2
            val left = state.servingSide == Side.LEFT
            val x = layout.getInt(if (left) "leftArrowX" else "rightArrowX")
            val top = layout.getInt(if (count == 1) "arrowSingleTopY" else "arrowDoubleTopY")
            repeat(count) { index ->
                arrow.forEachIndexed { row, bits ->
                    (if (left) bits.reversed() else bits).forEachIndexed { col, bit ->
                        if (bit == '1') pixel(x + col, top + index * (arrow.size + layout.getInt("arrowGap")) + row)
                    }
                }
            }
        }

    private fun rows(
        json: JSONObject,
        key: String,
    ): List<String> {
        val array = json.getJSONArray(key)
        return (0 until array.length()).map { array.getString(it) }
    }
}
