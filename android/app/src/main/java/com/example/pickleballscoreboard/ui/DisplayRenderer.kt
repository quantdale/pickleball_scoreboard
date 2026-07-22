package com.example.pickleballscoreboard.ui

import android.content.Context
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.DrawScope
import com.example.pickleballscoreboard.state.ScoreboardState
import com.example.pickleballscoreboard.state.Side
import org.json.JSONObject

// Renders a 64×32 logical scoreboard preview matching the physical LED panel
// layout defined in docs/specs/03-display-rendering.md.
// Glyph data is loaded from the shared JSON assets (Spec 03 Section 6).
class DisplayRenderer(context: Context) {
    private val font: Map<Char, List<Int>>
    private val arrowRight: List<Int>

    init {
        val fontJson = context.assets.open("font_5x7.json").bufferedReader().use { it.readText() }
        val fontObj = JSONObject(fontJson)
        font = (0..9).associate { digit ->
            digit.toString()[0] to loadGlyph(fontObj, digit.toString(), DIGIT_WIDTH)
        }

        val arrowsJson = context.assets.open("arrows.json").bufferedReader().use { it.readText() }
        val arrowsObj = JSONObject(arrowsJson)
        arrowRight = loadGlyph(arrowsObj, "ARROW_RIGHT", ARROW_WIDTH)
    }

    private fun loadGlyph(json: JSONObject, key: String, width: Int): List<Int> {
        val rows = json.getJSONArray(key)
        return (0 until rows.length()).map { i ->
            rows.getString(i).foldIndexed(0) { col, acc, ch ->
                if (ch == '1') acc or (1 shl (width - 1 - col)) else acc
            }
        }
    }

    // Entry point called inside a Compose Canvas DrawScope.
    fun DrawScope.render(state: ScoreboardState) {
        val pixelSize = size.width / CANVAS_WIDTH

        // Spec 03 Section 4c.
        drawDivider(pixelSize)

        // Spec 03 Section 4a.
        drawScore(state.leftScore, LEFT_CENTER_X, DIGIT_TOP_Y, pixelSize)
        drawScore(state.rightScore, RIGHT_CENTER_X, DIGIT_TOP_Y, pixelSize)

        // Spec 03 Section 4b.
        if (state.servingSide == Side.LEFT) {
            drawArrows(Side.LEFT, state.serverNumber, LEFT_CENTER_X, ARROW_TOP_Y, pixelSize)
        } else {
            drawArrows(Side.RIGHT, state.serverNumber, RIGHT_CENTER_X, ARROW_TOP_Y, pixelSize)
        }
    }

    private fun DrawScope.drawPixel(x: Int, y: Int, pixelSize: Float, color: Color = Color.White) {
        drawRect(
            color = color,
            topLeft = Offset(x * pixelSize, y * pixelSize),
            size = Size(pixelSize, pixelSize)
        )
    }

    private fun DrawScope.drawScore(score: Int, centerX: Float, topY: Int, pixelSize: Float) {
        val digits = score.toString().toList()
        val totalWidth = digits.size * DIGIT_WIDTH + (digits.size - 1) * DIGIT_GAP
        val startX = (centerX - totalWidth / 2.0f + 0.5f).toInt()
        digits.forEachIndexed { index, digit ->
            val glyph = font[digit] ?: return@forEachIndexed
            val x = startX + index * (DIGIT_WIDTH + DIGIT_GAP)
            drawGlyph(glyph, x, topY, DIGIT_WIDTH, pixelSize)
        }
    }

    private fun DrawScope.drawGlyph(rows: List<Int>, x: Int, y: Int, width: Int, pixelSize: Float) {
        rows.forEachIndexed { rowIndex, rowBits ->
            (0 until width).forEach { col ->
                if ((rowBits shr (width - 1 - col)) and 1 == 1) {
                    drawPixel(x + col, y + rowIndex, pixelSize)
                }
            }
        }
    }

    private fun DrawScope.drawArrows(
        side: Side,
        count: Int,
        centerX: Float,
        topY: Int,
        pixelSize: Float
    ) {
        if (count < 1) return
        val totalWidth = count * ARROW_WIDTH + (count - 1) * ARROW_GAP
        val startX = (centerX - totalWidth / 2.0f + 0.5f).toInt()
        repeat(count) { arrowIndex ->
            val x = startX + arrowIndex * (ARROW_WIDTH + ARROW_GAP)
            arrowRight.forEachIndexed { rowIndex, rowBits ->
                val bits = if (side == Side.LEFT) reverseBits(rowBits, ARROW_WIDTH) else rowBits
                (0 until ARROW_WIDTH).forEach { col ->
                    if ((bits shr (ARROW_WIDTH - 1 - col)) and 1 == 1) {
                        drawPixel(x + col, topY + rowIndex, pixelSize)
                    }
                }
            }
        }
    }

    private fun DrawScope.drawDivider(pixelSize: Float) {
        val dividerX = CANVAS_WIDTH / 2
        // Sparse/dotted line reads as "dim" on full-on/full-off LEDs and matches
        // the firmware rendering choice (Spec 03 Section 4c).
        for (y in 4..28 step 4) {
            drawPixel(dividerX, y, pixelSize)
        }
    }

    private fun reverseBits(value: Int, width: Int): Int {
        var reversed = 0
        var v = value
        repeat(width) {
            reversed = (reversed shl 1) or (v and 1)
            v = v shr 1
        }
        return reversed
    }

    companion object {
        private const val CANVAS_WIDTH = 64
        private const val CANVAS_HEIGHT = 32
        private const val LEFT_CENTER_X = 15.5f
        private const val RIGHT_CENTER_X = 47.5f
        private const val DIGIT_WIDTH = 5
        private const val DIGIT_HEIGHT = 7
        private const val DIGIT_GAP = 1
        private const val ARROW_WIDTH = 9
        private const val ARROW_HEIGHT = 7
        private const val ARROW_GAP = 1
        private const val ARROW_TOP_Y = 3
        private const val DIGIT_TOP_Y = 15
    }
}
