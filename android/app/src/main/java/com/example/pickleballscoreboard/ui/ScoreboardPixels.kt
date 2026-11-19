package com.example.pickleballscoreboard.ui

import com.example.pickleballscoreboard.state.GameMode
import com.example.pickleballscoreboard.state.ScoreboardState
import com.example.pickleballscoreboard.state.Side
import org.json.JSONObject

internal val ScoreboardState.servingScore: Int get() = if (servingSide == Side.LEFT) leftScore else rightScore
internal val ScoreboardState.opponentScore: Int get() = if (servingSide == Side.LEFT) rightScore else leftScore

data class LitPixel(
    val x: Int,
    val y: Int,
)

// Spec 03 Sections 4/6: pure portrait pixels, used by Compose and the JVM tests.
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
    private val servingTop = layout.getInt("servingTopY")
    private val opponentTop = layout.getInt("opponentTopY")
    private val serverTop = layout.getInt("serverTopY")
    private val scaleY = layout.getInt("digitScaleY")
    private val singleScaleX = layout.getInt("singleScaleX")
    private val doubleScaleX = layout.getInt("doubleScaleX")
    private val gap = layout.getInt("digitGap")
    private val arrowScale = layout.getInt("arrowScale")
    private val arrowTop = layout.getInt("arrowTopY")
    private val singlesServingTop = layout.getInt("singlesServingTopY")
    private val singlesOpponentTop = layout.getInt("singlesOpponentTopY")
    private val singlesArrowTop = layout.getInt("singlesArrowTopY")

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
                top: Int,
            ) {
                val digits = if (value in 0..99) value.toString() else "--"
                val scaleX = if (digits.length == 1) singleScaleX else doubleScaleX
                val glyphWidth = 5 * scaleX
                val start = (width - digits.length * glyphWidth - (digits.length - 1) * gap) / 2
                digits.forEachIndexed { index, digit ->
                    val x = start + index * (glyphWidth + gap)
                    if (digit == '-') {
                        block(x, top + 3 * scaleY, glyphWidth, scaleY)
                    } else {
                        font[digit.digitToInt()].forEachIndexed { row, bits ->
                            bits.forEachIndexed { col, bit ->
                                if (bit == '1') block(x + col * scaleX, top + row * scaleY, scaleX, scaleY)
                            }
                        }
                    }
                }
            }

            val singles = state.gameMode == GameMode.SINGLES
            number(state.servingScore, if (singles) singlesServingTop else servingTop)
            number(state.opponentScore, if (singles) singlesOpponentTop else opponentTop)
            if (!singles) number(state.serverNumber, serverTop)
            val arrowY = if (singles) singlesArrowTop else arrowTop
            arrow.forEachIndexed { row, bits ->
                val start = (width - bits.length * arrowScale) / 2
                bits.forEachIndexed { col, bit ->
                    val x = if (state.servingSide == Side.LEFT) bits.lastIndex - col else col
                    if (bit == '1') block(start + x * arrowScale, arrowY + row * arrowScale, arrowScale, arrowScale)
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
