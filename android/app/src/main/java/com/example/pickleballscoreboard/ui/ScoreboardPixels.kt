package com.example.pickleballscoreboard.ui

import com.example.pickleballscoreboard.state.GameMode
import com.example.pickleballscoreboard.state.ScoreboardState
import com.example.pickleballscoreboard.state.Side
import org.json.JSONObject

data class LitPixel(
    val x: Int,
    val y: Int,
)

// Spec 01 §5 / Spec 03 §4b: Singles always shows one arrow.
internal val ScoreboardState.arrowCount: Int get() = if (gameMode == GameMode.DOUBLES && serverNumber == 2) 2 else 1

// Spec 03 Sections 4/6: pure 64x16 landscape pixels, used by Compose and the JVM tests.
class ScoreboardPixels(
    fontJson: String,
    arrowsJson: String,
    layoutJson: String,
) {
    private val font = JSONObject(fontJson).let { json -> (0..9).map { rows(json, it.toString()) } }
    private val arrow = rows(JSONObject(arrowsJson), "ARROW_RIGHT")
    private val layout = JSONObject(layoutJson)
    val width = layout.getInt("canvasWidth")
    val height = layout.getInt("canvasHeight")
    private val digitTop = layout.getInt("digitTopY")
    private val scaleX = layout.getInt("digitScaleX")
    private val scaleY = layout.getInt("digitScaleY")
    private val digitGap = layout.getInt("digitGap")
    private val leftScoreX = layout.getInt("leftScoreX")
    private val rightScoreX = layout.getInt("rightScoreX")
    private val scoreWidth = layout.getInt("scoreWidth")
    private val arrowTop = layout.getInt("arrowTopY")
    private val arrowGap = layout.getInt("arrowGap")

    fun pixels(state: ScoreboardState): Set<LitPixel> =
        buildSet {
            fun block(
                x: Int,
                y: Int,
                w: Int,
                h: Int,
            ) {
                for (dy in 0 until h) {
                    for (dx in 0 until w) {
                        val px = x + dx
                        val py = y + dy
                        if (px in 1 until (width - 1) && py in 1 until (height - 1)) add(LitPixel(px, py))
                    }
                }
            }

            fun number(
                value: Int,
                regionX: Int,
            ) {
                val digits = if (value in 0..99) value.toString() else "--"
                val glyphWidth = 5 * scaleX
                val start = regionX + (scoreWidth - digits.length * glyphWidth - (digits.length - 1) * digitGap) / 2
                digits.forEachIndexed { index, digit ->
                    val x = start + index * (glyphWidth + digitGap)
                    if (digit == '-') {
                        block(x, digitTop + 3 * scaleY, glyphWidth, scaleY)
                    } else {
                        font[digit.digitToInt()].forEachIndexed { row, bits ->
                            bits.forEachIndexed { col, bit ->
                                if (bit == '1') block(x + col * scaleX, digitTop + row * scaleY, scaleX, scaleY)
                            }
                        }
                    }
                }
            }

            number(state.leftScore, leftScoreX)
            number(state.rightScore, rightScoreX)
            val arrowWidth = arrow.first().length
            val count = state.arrowCount
            val start = (width - count * arrowWidth - (count - 1) * arrowGap) / 2
            repeat(count) { index ->
                val x0 = start + index * (arrowWidth + arrowGap)
                arrow.forEachIndexed { row, bits ->
                    bits.forEachIndexed { col, _ ->
                        val source = if (state.servingSide == Side.LEFT) bits.lastIndex - col else col
                        if (bits[source] == '1') block(x0 + col, arrowTop + row, 1, 1)
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
