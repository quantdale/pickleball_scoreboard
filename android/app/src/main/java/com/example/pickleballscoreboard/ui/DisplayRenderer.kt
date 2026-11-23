package com.example.pickleballscoreboard.ui

import android.content.Context
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Paint
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.drawIntoCanvas
import com.example.pickleballscoreboard.state.ScoreboardState

// Spec 03 Section 6: actual Compose drawing uses the same tested logical pixel set.
class DisplayRenderer(
    context: Context,
) {
    private val renderer =
        ScoreboardPixels(
            context.assets
                .open("font_5x7.json")
                .bufferedReader()
                .use { it.readText() },
            context.assets
                .open("arrows.json")
                .bufferedReader()
                .use { it.readText() },
            context.assets
                .open("layout.json")
                .bufferedReader()
                .use { it.readText() },
        )
    val aspectRatio: Float = renderer.width.toFloat() / renderer.height
    private val litPixel =
        Paint().apply {
            isAntiAlias = false // Spec 03 Section 3: no gray grid between adjacent pixels.
        }

    fun pixels(state: ScoreboardState): Set<LitPixel> = renderer.pixels(state)

    // Spec 03 §3: every lit pixel uses the board's one color.
    fun DrawScope.render(
        pixels: Set<LitPixel>,
        color: Color = Color.White,
    ) {
        litPixel.color = color
        drawRect(Color.Black)
        val scale = minOf(size.width / renderer.width, size.height / renderer.height)
        drawIntoCanvas { canvas ->
            pixels.forEach { pixel ->
                canvas.drawRect(
                    pixel.x * scale,
                    pixel.y * scale,
                    (pixel.x + 1) * scale,
                    (pixel.y + 1) * scale,
                    litPixel,
                )
            }
        }
    }
}
