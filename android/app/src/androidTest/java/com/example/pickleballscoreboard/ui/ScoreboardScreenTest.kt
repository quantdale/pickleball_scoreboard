package com.example.pickleballscoreboard.ui

import android.graphics.Bitmap
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asAndroidBitmap
import androidx.compose.ui.graphics.toPixelMap
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.SemanticsProperties
import androidx.compose.ui.test.*
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.unit.dp
import androidx.test.platform.app.InstrumentationRegistry
import com.example.pickleballscoreboard.R
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

    private fun gameMenu() = text("Board").performClick()

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
    fun previewStaysLeftOfStackedControlsAndMatchesAllLogicalPixels() {
        fun asset(name: String) =
            context.assets
                .open(name)
                .bufferedReader()
                .use { it.readText() }
        val renderer = ScoreboardPixels(asset("font_5x7.json"), asset("arrows.json"), asset("layout.json"))
        show()
        val left = text("Rally Left").assertIsDisplayed().getUnclippedBoundsInRoot()
        val right = text("Rally Right").assertIsDisplayed().getUnclippedBoundsInRoot()
        val board = compose.onNodeWithTag("portrait-preview").assertIsDisplayed()
        assertTrue(board.getUnclippedBoundsInRoot().right < left.left)
        assertEquals(left.left, right.left)
        assertTrue(left.bottom < right.top)
        assertTrue(left.bottom - left.top >= 63.dp)
        assertTrue(right.bottom - right.top >= 63.dp)
        var bottom = right.bottom
        for (label in listOf("Undo", "Switch Courts", "End Game", "Reset")) {
            val action = text(label).assert(SemanticsMatcher.expectValue(SemanticsProperties.Role, Role.Button))
            val bounds = action.getUnclippedBoundsInRoot()
            assertTrue("$label placement", bounds.left == left.left && bounds.top > bottom)
            assertTrue("$label touch target", bounds.bottom - bounds.top >= 47.5.dp)
            bottom = bounds.bottom
        }
        save("doubles", compose.onNodeWithTag("controller"))
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
        text("2 commands queued or sending").assertIsDisplayed()
        update("3,0,R,1,0,D,1")
        repeat(3) { tap("Undo") }
        text("Undo").assertIsEnabled()
        update("0,0,L,2,0,D,0")
        text("Undo").assertIsNotEnabled()
        tap("Switch Courts")
        update("0,0,R,2,0,D,3")
        text("Board memory is low. Game action was not recorded.").assertIsDisplayed()
        compose.runOnIdle { assertEquals(listOf("L", "L", "L", "R", "U", "U", "U", "C"), events) }
    }

    @Test
    fun commandFeedbackDoesNotMoveTheLiveBoardOrControls() {
        show()
        val board = compose.onNodeWithTag("portrait-preview")
        val actions = listOf("Rally Left", "Rally Right", "Undo", "Switch Courts", "End Game", "Reset")
        val anchors = listOf(text("Board"), board) + actions.map(::text)
        val idleBounds = anchors.map { it.getUnclippedBoundsInRoot() }
        save("status-idle", compose.onNodeWithTag("controller"))

        repeat(3) { round ->
            for (action in actions.take(4)) {
                tap(action)
                for (pending in listOf(1, 3, 0)) {
                    compose.runOnIdle { status.value = status.value.copy(pendingWrites = pending) }
                    if (pending > 0) text(status.value.transportText).assertIsDisplayed()
                    if (round == 0 && action == "Rally Left" && pending == 1) {
                        save("status-queued", compose.onNodeWithTag("controller"))
                    }
                    assertEquals("after $action, pending=$pending", idleBounds, anchors.map { it.getUnclippedBoundsInRoot() })
                }
            }
        }

        update("2,3,L,2,0,D,3")
        text("Board memory is low. Game action was not recorded.").assertIsDisplayed()
        assertEquals(idleBounds, anchors.map { it.getUnclippedBoundsInRoot() })
        compose.runOnIdle { displayed.value = checkNotNull(parseStatePayload("2,3,L,2,0,D")) }
        text("Legacy board: Singles layout or Undo may differ.").assertIsDisplayed()
        assertEquals(idleBounds, anchors.map { it.getUnclippedBoundsInRoot() })
        text("Connected").assertIsDisplayed()
    }

    @Test
    fun endAndResetRequireConfirmationAndLegacyUndoStillWorks() {
        show()
        tap("End Game")
        tap("Cancel")
        compose.runOnIdle { assertTrue(events.isEmpty()) }
        tap("End Game")
        tap("End game now")
        update("2,3,R,2,1,D,1")
        text("Game ended").assertIsDisplayed()
        text("Rally Left").assertIsNotEnabled()
        text("Undo").assertIsEnabled()
        tap("Reset")
        text("Reset score").assertIsNotEnabled()
        tap("Singles")
        tap("Right serves first")
        tap("Reset score")
        text("Game ended").assertIsDisplayed() // no local score or mode change
        update("0,0,R,2,0,S,1")
        text("Singles").assertIsDisplayed()
        text("Rally Right").assertIsEnabled()
        tap("Reset")
        tap("Doubles")
        tap("Left serves first")
        tap("Reset score")
        compose.runOnIdle { displayed.value = checkNotNull(parseStatePayload("0,0,L,2,0,D")) }
        text("Legacy board: Singles layout or Undo may differ.").assertIsDisplayed()
        text("Undo").performScrollTo().assertIsEnabled().performTouchInput { click() }
        gameMenu()
        tap("Disconnect")
        compose.runOnIdle { assertEquals(listOf("E", "0RS", "0LD", "U", "disconnect"), events) }
    }

    @Test
    fun losingSynchronizationClosesDialogAndGatesCommandsUntilResync() {
        show()
        tap("Reset")
        for (phase in ConnectionPhase.values().filter { it != ConnectionPhase.Ready }) {
            compose.runOnIdle { status.value = ControllerStatus(phase) }
            text(phase.label).assertIsDisplayed()
            text("Reset game?").assertDoesNotExist()
            compose.onNodeWithContentDescription("Scoreboard: no live score").assertExists()
            for (action in listOf("Rally Left", "Rally Right", "Undo", "Switch Courts", "End Game", "Reset")) {
                text(action).assertIsNotEnabled()
            }
        }
        save("reconnect", compose.onNodeWithTag("controller"))
        compose.runOnIdle { status.value = ControllerStatus(ConnectionPhase.Ready) }
        text("Rally Right").assertIsEnabled()
        text("Undo").assertIsEnabled()
        compose.runOnIdle { assertTrue(events.isEmpty()) }
    }

    @Test
    fun boardMenuShowsPackagedVersionEvenWhenDisconnected() {
        status.value = ControllerStatus()
        show()
        gameMenu()
        text(context.getString(R.string.controller_version)).assertIsDisplayed()
        text("Disconnect").assertDoesNotExist()
        text("Maintenance").assertDoesNotExist()
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
