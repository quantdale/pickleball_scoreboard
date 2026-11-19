package com.example.pickleballscoreboard.ui

import android.graphics.Bitmap
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asAndroidBitmap
import androidx.compose.ui.graphics.toPixelMap
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.assertIsEnabled
import androidx.compose.ui.test.assertIsNotEnabled
import androidx.compose.ui.test.captureToImage
import androidx.compose.ui.test.getUnclippedBoundsInRoot
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.unit.dp
import androidx.test.platform.app.InstrumentationRegistry
import com.example.pickleballscoreboard.ble.parseStatePayload
import com.example.pickleballscoreboard.state.ScoreboardState
import com.example.pickleballscoreboard.state.Side
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import java.io.File

// Local UI/renderer verification only: no simulated connection in the production APK.
class ScoreboardScreenTest {
    @get:Rule val compose = createComposeRule()
    private val context get() = InstrumentationRegistry.getInstrumentation().targetContext
    private val displayed = mutableStateOf(ScoreboardState(2, 3))
    private val status = mutableStateOf(ControllerStatus(ConnectionPhase.Ready))
    private val events = mutableListOf<String>()

    private fun show() {
        compose.setContent {
            CourtTheme {
                Box(Modifier.fillMaxSize().testTag("controller")) {
                    ScoreboardScreen(
                        displayed.value,
                        status.value,
                        onDisconnect = { events.add("disconnect") },
                        onRallyLeft = { events.add("L") },
                        onRallyRight = { events.add("R") },
                        onUndo = { events.add("U") },
                        onSwitchCourts = { events.add("C") },
                        onEndGame = { events.add("E") },
                        onReset = { side, mode -> events.add("0${side.name.first()}${mode.name.first()}") },
                    )
                }
            }
        }
    }

    @Test
    fun canvasMatchesAllLogicalPixelsAndPrimaryTargetsAreOnTheRight() {
        fun asset(name: String) =
            context.assets
                .open(name)
                .bufferedReader()
                .use { it.readText() }
        val renderer = ScoreboardPixels(asset("font_5x7.json"), asset("arrows.json"), asset("layout.json"))
        show()
        val states = listOf(ScoreboardState(), ScoreboardState(2, 3), ScoreboardState(10, 25, Side.RIGHT, 1), ScoreboardState(88, 99))
        val board = compose.onNodeWithTag("portrait-preview")
        val left = compose.onNodeWithText("Rally Left").assertIsDisplayed()
        val right = compose.onNodeWithText("Rally Right").assertIsDisplayed()
        val leftBounds = left.getUnclippedBoundsInRoot()
        val rightBounds = right.getUnclippedBoundsInRoot()
        assertTrue(leftBounds.left > board.getUnclippedBoundsInRoot().right)
        assertTrue(leftBounds.bottom < rightBounds.top)
        assertTrue(leftBounds.bottom - leftBounds.top >= 63.5.dp)
        assertTrue(rightBounds.bottom - rightBounds.top >= 63.5.dp)
        for (state in states) {
            compose.runOnIdle { displayed.value = state }
            val image = board.captureToImage()
            assertEquals(0.25, image.width.toDouble() / image.height, 0.003)
            val actual = image.toPixelMap()
            val lit = renderer.pixels(state)
            for (py in 0 until image.height) {
                for (px in 0 until image.width) {
                    val color = actual[px, py]
                    assertTrue((color.red == 0f || color.red == 1f) && color.green == color.red && color.blue == color.red)
                }
            }
            for (y in 0 until 64) {
                for (x in 0 until 16) {
                    val color = actual[((x + 0.5) * image.width / 16).toInt(), ((y + 0.5) * image.height / 64).toInt()]
                    assertEquals("pixel $x,$y", if (LitPixel(x, y) in lit) 1f else 0f, color.red, 0f)
                }
            }
        }
        saveController("ready")
    }

    @Test
    fun rallyTapsDoNotScoreLocallyAndDestructiveActionsRequireConfirmation() {
        show()
        repeat(3) { compose.onNodeWithText("Rally Left").performClick() }
        compose.onNodeWithText("Rally Right").performClick()
        compose.onNodeWithText("Undo").performClick()
        compose.onNodeWithText("Switch Courts").performScrollTo().performClick()
        val startingScore = "Scoreboard: serving score 2, opponent 3, server 2, left serving"
        compose.onNodeWithContentDescription(startingScore).assertExists()
        compose.runOnIdle { status.value = ControllerStatus(ConnectionPhase.Ready, pendingWrites = 2, submitted = true) }
        compose.onNodeWithText("2 commands queued or sending").assertIsDisplayed()
        compose.runOnIdle { status.value = status.value.copy(pendingWrites = 0) }
        compose.onNodeWithText("Write acknowledged · score updates separately").assertIsDisplayed()
        compose.onNodeWithContentDescription(startingScore).assertExists()
        compose.runOnIdle { displayed.value = checkNotNull(parseStatePayload("2,3,R,1,0,D")) }
        compose.onNodeWithContentDescription("Scoreboard: serving score 3, opponent 2, server 1, right serving").assertExists()
        compose.runOnIdle { displayed.value = checkNotNull(parseStatePayload("2,3,R,2,0,D")) }
        compose.onNodeWithContentDescription("Scoreboard: serving score 3, opponent 2, server 2, right serving").assertExists()

        compose.onNodeWithText("End Game").performScrollTo().performClick()
        compose.onNodeWithText("Cancel").performClick()
        compose.runOnIdle { assertEquals(listOf("L", "L", "L", "R", "U", "C"), events) }
        compose.onNodeWithText("End Game").performClick()
        compose.onNodeWithText("End game now").performClick()
        compose.runOnIdle { displayed.value = displayed.value.copy(gameEnded = true) }
        compose.onNodeWithText("Rally Left").assertIsNotEnabled()
        compose.onNodeWithText("Undo").assertIsEnabled()
        compose.onNodeWithText("Reset").performScrollTo().performClick()
        compose.onNodeWithText("Reset score").assertIsNotEnabled()
        compose.onNodeWithText("Singles").performScrollTo().performClick()
        compose.onNodeWithText("Right serves first").performScrollTo().performClick()
        compose.onNodeWithText("Reset score").performClick()
        compose.onNodeWithText("Board").performClick()
        compose.onNodeWithText("Disconnect").performClick()
        compose.runOnIdle { assertEquals(listOf("L", "L", "L", "R", "U", "C", "E", "0RS", "disconnect"), events) }
    }

    @Test
    fun synchronizationAndReconnectGateControlsAndDismissPendingConfirmation() {
        show()
        compose.onNodeWithText("Reset").performScrollTo().performClick()
        for (phase in ConnectionPhase.values().filter { it != ConnectionPhase.Ready }) {
            compose.runOnIdle { status.value = ControllerStatus(phase) }
            compose.onNodeWithText(phase.label).assertIsDisplayed()
            compose.onNodeWithText("Reset game?").assertDoesNotExist()
            compose.onNodeWithContentDescription("Scoreboard: no live score").assertExists()
            listOf("Rally Left", "Rally Right", "Undo", "Switch Courts", "End Game", "Reset").forEach {
                compose.onNodeWithText(it).assertIsNotEnabled()
            }
        }
        saveController("reconnect")
        compose.runOnIdle { status.value = ControllerStatus(ConnectionPhase.Ready) }
        compose.onNodeWithText("Rally Right").assertIsEnabled()
        compose.runOnIdle { assertTrue(events.isEmpty()) }
    }

    private fun saveController(name: String) {
        val image = compose.onNodeWithTag("controller").captureToImage()
        File(context.getExternalFilesDir(null), "$name.png").outputStream().use {
            image.asAndroidBitmap().compress(Bitmap.CompressFormat.PNG, 100, it)
        }
    }
}
