package com.example.pickleballscoreboard.ui

import android.graphics.Bitmap
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.graphics.asAndroidBitmap
import androidx.compose.ui.graphics.toPixelMap
import androidx.compose.ui.test.captureToImage
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.test.platform.app.InstrumentationRegistry
import com.example.pickleballscoreboard.state.ScoreboardState
import com.example.pickleballscoreboard.state.Side
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import java.io.File

// Local UI/renderer verification only: no simulated production BLE connection.
class ScoreboardScreenTest {
    @get:Rule
    val compose = createComposeRule()

    @Test
    fun actualCanvasMatchesLogicalPixelsAndControlsDispatchInputs() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext

        fun asset(name: String) =
            context.assets
                .open(name)
                .bufferedReader()
                .use { it.readText() }
        val renderer = ScoreboardPixels(asset("font_5x7.json"), asset("arrows.json"), asset("layout.json"))
        val displayed = mutableStateOf(ScoreboardState())
        val events = mutableListOf<String>()
        compose.setContent {
            MaterialTheme {
                ScoreboardScreen(
                    displayed.value,
                    "Renderer test (not BLE)",
                    onDisconnect = { events.add("disconnect") },
                    onRallyLeft = { events.add("L") },
                    onRallyRight = { events.add("R") },
                    onUndo = { events.add("U") },
                    onSwitchCourts = { events.add("C") },
                    onEndGame = { events.add("E") },
                    onReset = { events.add("0") },
                )
            }
        }
        val states = listOf(ScoreboardState(), ScoreboardState(10, 25, Side.RIGHT, 1), ScoreboardState(99, 99))
        for (state in states) {
            compose.runOnIdle { displayed.value = state }
            val image = compose.onNodeWithContentDescription("Scoreboard:", substring = true).captureToImage()
            assertEquals(4.0, image.width.toDouble() / image.height, 0.03)
            val actual = image.toPixelMap()
            // Fractional scaling must not introduce a gray grid inside white glyphs.
            for (py in 0 until image.height) {
                for (px in 0 until image.width) {
                    val color = actual[px, py]
                    assertTrue((color.red == 0f || color.red == 1f) && color.green == color.red && color.blue == color.red)
                }
            }
            val lit = renderer.pixels(state)
            for (y in 0 until 16) {
                for (x in 0 until 64) {
                    val color = actual[((x + 0.5) * image.width / 64).toInt(), ((y + 0.5) * image.height / 16).toInt()]
                    val expected = if (LitPixel(x, y) in lit) 1f else 0f
                    assertEquals("pixel $x,$y", expected, color.red, 0.01f)
                    assertEquals(expected, color.green, 0.01f)
                    assertEquals(expected, color.blue, 0.01f)
                }
            }
            val file = File(context.getExternalFilesDir(null), "preview-${state.leftScore}-${state.rightScore}.png")
            file.outputStream().use { image.asAndroidBitmap().compress(Bitmap.CompressFormat.PNG, 100, it) }
        }
        for (label in listOf("Rally Left", "Rally Right", "Undo", "Switch Courts", "End Game", "Reset", "Disconnect")) {
            compose.onNodeWithText(label).performClick()
        }
        compose.runOnIdle { assertTrue(events == listOf("L", "R", "U", "C", "E", "0", "disconnect")) }
    }
}
