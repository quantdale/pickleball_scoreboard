package com.example.pickleballscoreboard.ui

import android.graphics.Bitmap
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asAndroidBitmap
import androidx.compose.ui.graphics.toPixelMap
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.test.*
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.unit.dp
import androidx.test.platform.app.InstrumentationRegistry
import com.example.pickleballscoreboard.ble.parseStatePayload
import com.example.pickleballscoreboard.state.ScoreboardState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import java.io.File

// Emulator verification only: no simulated connection in the production APK.
class ScoreboardScreenTest {
    @get:Rule val compose = createComposeRule()
    private val context get() = InstrumentationRegistry.getInstrumentation().targetContext
    private val displayed = mutableStateOf(ScoreboardState(2, 3, canUndo = true))
    private val status = mutableStateOf(ControllerStatus(ConnectionPhase.Ready))
    private val events = mutableListOf<String>()

    private fun text(value: String) = compose.onNodeWithText(value)

    private fun tap(value: String) = text(value).performClick()

    private fun gameMenu() = compose.onNodeWithContentDescription("Game menu").performClick()

    private fun update(payload: String) =
        compose.runOnIdle {
            displayed.value = checkNotNull(parseStatePayload(payload, extended = true))
        }

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
    fun courtUsesPairedLargeTargetsAndPreviewMatchesAllLogicalPixels() {
        fun asset(name: String) =
            context.assets
                .open(name)
                .bufferedReader()
                .use { it.readText() }
        val renderer = ScoreboardPixels(asset("font_5x7.json"), asset("arrows.json"), asset("layout.json"))
        show()
        val left = text("Rally Left").assertIsDisplayed().getUnclippedBoundsInRoot()
        val right = text("Rally Right").assertIsDisplayed().getUnclippedBoundsInRoot()
        assertTrue(left.right < right.left)
        assertEquals(left.top, right.top)
        assertTrue(left.bottom - left.top >= 85.dp)
        assertTrue(right.bottom - right.top >= 85.dp)
        save("doubles", compose.onNodeWithTag("controller"))
        tap("LED preview")
        val board = compose.onNodeWithTag("portrait-preview")
        for (payload in listOf(
            "0,0,L,2,0,D,0",
            "10,25,R,1,0,D,1",
            "88,99,L,2,0,D,1",
            "4,4,L,2,0,S,1",
            "4,4,L,1,0,S,1",
            "25,3,R,2,0,S,1",
            "98,100,R,2,1,S,1",
        )) {
            update(payload)
            val image = board.captureToImage()
            assertEquals(0.25, image.width.toDouble() / image.height, 0.004)
            val actual = image.toPixelMap()
            val lit = renderer.pixels(displayed.value)
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
        update("4,4,L,2,0,S,1")
        save("singles-led", board)
        tap("Close")
        save("singles", compose.onNodeWithTag("controller"))
    }

    @Test
    fun controlsDispatchOnlyAndUndoTracksAuthoritativeHistory() {
        show()
        update("0,0,L,2,0,D,0")
        text("Undo").assertIsNotEnabled()
        repeat(3) { tap("Rally Left") }
        tap("Rally Right")
        text("Undo").assertIsNotEnabled()
        compose.runOnIdle { status.value = status.value.copy(pendingWrites = 2) }
        text("2 commands queued or sending").performScrollTo().assertIsDisplayed()
        update("3,0,R,1,0,D,1")
        repeat(3) { tap("Undo") }
        text("Undo").assertIsEnabled()
        update("0,0,L,2,0,D,0")
        text("Undo").assertIsNotEnabled()
        tap("Switch Courts")
        update("0,0,R,2,0,D,3")
        text("Board memory is low. Game action was not recorded.").performScrollTo().assertIsDisplayed()
        compose.runOnIdle { assertEquals(listOf("L", "L", "L", "R", "U", "U", "U", "C"), events) }
    }

    @Test
    fun endAndResetRequireConfirmationAndLegacyUndoStillWorks() {
        show()
        gameMenu()
        tap("End Game")
        tap("Cancel")
        compose.runOnIdle { assertTrue(events.isEmpty()) }
        gameMenu()
        tap("End Game")
        tap("End game now")
        update("2,3,R,2,1,D,1")
        text("Final score").assertIsDisplayed()
        text("Rally Left").assertIsNotEnabled()
        text("Undo").assertIsEnabled()
        gameMenu()
        tap("Reset")
        text("Reset score").assertIsNotEnabled()
        tap("Singles")
        tap("Right serves first")
        tap("Reset score")
        text("Final score").assertIsDisplayed() // no local score or mode change
        update("0,0,R,2,0,S,1")
        text("Singles").assertIsDisplayed()
        text("Rally Right").assertIsEnabled()
        gameMenu()
        tap("Reset")
        tap("Doubles")
        tap("Left serves first")
        tap("Reset score")
        compose.runOnIdle { displayed.value = checkNotNull(parseStatePayload("0,0,L,2,0,D")) }
        text("Undo").performScrollTo().assertIsEnabled().performTouchInput { click() }
        tap("LED preview")
        text("Legacy board: the Singles layout may differ and Undo may be limited.").assertIsDisplayed()
        tap("Close")
        gameMenu()
        tap("Disconnect")
        compose.runOnIdle { assertEquals(listOf("E", "0RS", "0LD", "U", "disconnect"), events) }
    }

    @Test
    fun losingSynchronizationClosesDialogAndGatesCommandsUntilResync() {
        show()
        gameMenu()
        tap("Reset")
        for (phase in ConnectionPhase.values().filter { it != ConnectionPhase.Ready }) {
            compose.runOnIdle { status.value = ControllerStatus(phase) }
            text(phase.label).assertIsDisplayed()
            text("Reset game?").assertDoesNotExist()
            compose.onNodeWithTag("portrait-preview").assertDoesNotExist()
            for (action in listOf("Rally Left", "Rally Right", "Undo", "Switch Courts", "LED preview")) {
                text(action).assertIsNotEnabled()
            }
            gameMenu()
            text("Reset").assertIsNotEnabled()
            text("End Game").assertIsNotEnabled()
            InstrumentationRegistry.getInstrumentation().uiAutomation.performGlobalAction(
                android.accessibilityservice.AccessibilityService.GLOBAL_ACTION_BACK,
            )
        }
        save("reconnect", compose.onNodeWithTag("controller"))
        compose.runOnIdle { status.value = ControllerStatus(ConnectionPhase.Ready) }
        text("Rally Right").assertIsEnabled()
        text("Undo").assertIsEnabled()
        compose.runOnIdle { assertTrue(events.isEmpty()) }
    }

    private fun save(
        name: String,
        node: SemanticsNodeInteraction,
    ) {
        File(context.getExternalFilesDir(null), "$name.png").outputStream().use {
            node.captureToImage().asAndroidBitmap().compress(Bitmap.CompressFormat.PNG, 100, it)
        }
    }
}
