package com.example.pickleballscoreboard.ui

import android.app.UiAutomation
import android.graphics.Bitmap
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asAndroidBitmap
import androidx.compose.ui.graphics.toPixelMap
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.SemanticsProperties
import androidx.compose.ui.test.*
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.dp
import androidx.test.platform.app.InstrumentationRegistry
import com.example.pickleballscoreboard.R
import com.example.pickleballscoreboard.ble.parseStatePayload
import org.junit.rules.ExternalResource
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import java.io.File

// Emulator verification only: no simulated connection in the production APK.
class ScoreboardScreenTest {
    // Freeze landscape BEFORE the host activity launches; rotating later recreates it.
    @get:Rule(order = 0)
    val landscape =
        object : ExternalResource() {
            override fun before() {
                InstrumentationRegistry.getInstrumentation().uiAutomation.setRotation(UiAutomation.ROTATION_FREEZE_90)
                InstrumentationRegistry.getInstrumentation().waitForIdleSync()
            }

            override fun after() {
                InstrumentationRegistry.getInstrumentation().uiAutomation.setRotation(UiAutomation.ROTATION_UNFREEZE)
            }
        }

    @get:Rule(order = 1)
    val compose = createComposeRule()
    private val instrumentation get() = InstrumentationRegistry.getInstrumentation()
    private val context get() = instrumentation.targetContext
    private val displayed = mutableStateOf(checkNotNull(parseStatePayload("7,5,L,1,0,D,1")))
    private val status = mutableStateOf(ControllerStatus(ConnectionPhase.Ready))
    private val fontScale = mutableFloatStateOf(1f)
    private val events = mutableListOf<String>()

    private fun text(value: String) = compose.onNodeWithText(value)

    private fun key(description: String) = compose.onNodeWithContentDescription(description)

    private fun update(payload: String) = compose.runOnIdle { displayed.value = checkNotNull(parseStatePayload(payload)) }

    private fun show() {
        compose.setContent {
            val density = LocalDensity.current
            CompositionLocalProvider(LocalDensity provides Density(density.density, fontScale.floatValue)) {
                CourtTheme {
                    Box(Modifier.fillMaxSize().testTag("controller")) {
                        ScoreboardScreen(
                            displayed.value,
                            status.value,
                            onDisconnect = { events.add("disconnect") },
                            onCommand = { events.add(it.wire) },
                        )
                    }
                }
            }
        }
    }

    @Test
    fun layoutFollowsTheOwnerSketchAtNormalAndLargeText() {
        show()
        for (scale in listOf(1f, 1.35f)) {
            compose.runOnIdle { fontScale.floatValue = scale }
            val root = compose.onNodeWithTag("controller").getUnclippedBoundsInRoot()
            assertTrue("landscape", root.right - root.left > root.bottom - root.top)
            val leftPlus = key("Left plus").getUnclippedBoundsInRoot()
            val leftScore = key("Left score 7").assertIsDisplayed().getUnclippedBoundsInRoot()
            val leftMinus = key("Left minus").getUnclippedBoundsInRoot()
            val serve = compose.onNodeWithTag("serve").assertIsDisplayed().getUnclippedBoundsInRoot()
            val rightPlus = key("Right plus").getUnclippedBoundsInRoot()
            val rightScore = key("Right score 5").assertIsDisplayed().getUnclippedBoundsInRoot()
            val rightMinus = key("Right minus").getUnclippedBoundsInRoot()
            // + above score above −, per side; left section | serve | right section.
            assertTrue(leftPlus.bottom <= leftScore.top && leftScore.bottom <= leftMinus.top)
            assertTrue(rightPlus.bottom <= rightScore.top && rightScore.bottom <= rightMinus.top)
            assertTrue(leftPlus.right < serve.left && serve.right < rightPlus.left)
            assertEquals(leftPlus.top, rightPlus.top)
            assertTrue("serve is prominent", serve.bottom - serve.top >= leftScore.bottom - leftScore.top)
            for (bounds in listOf(leftPlus, leftMinus, rightPlus, rightMinus)) {
                assertTrue("score key height", bounds.bottom - bounds.top >= 52.dp)
            }
            // Bottom section, left to right: Undo, Switch Court, End Game, Reset.
            var previousRight = root.left
            for (label in listOf("Undo", "Switch Court", "End Game", "Reset")) {
                val bounds =
                    text(label)
                        .assert(SemanticsMatcher.expectValue(SemanticsProperties.Role, Role.Button))
                        .assertIsDisplayed()
                        .getUnclippedBoundsInRoot()
                assertTrue("$label below the scores", bounds.top >= leftMinus.bottom)
                assertTrue("$label order", bounds.left >= previousRight)
                assertTrue("$label touch target", bounds.bottom - bounds.top >= 47.5.dp)
                assertTrue("$label on screen", bounds.bottom <= root.bottom && bounds.right <= root.right)
                previousRight = bounds.right
            }
            save(if (scale == 1f) "landscape-doubles" else "landscape-font-135", compose.onNodeWithTag("controller"))
        }
    }

    @Test
    fun ledPreviewMatchesEveryLogicalBoardPixel() {
        fun asset(name: String) = context.assets.open(name).bufferedReader().use { it.readText() }
        val renderer = ScoreboardPixels(asset("font_5x7.json"), asset("arrows.json"), asset("layout.json"))
        show()
        val board = compose.onNodeWithTag("led-preview").assertIsDisplayed()
        for (payload in listOf("0,0,L,1,0,D,0", "5,2,L,2,0,D,1", "3,3,R,1,0,D,1", "10,25,R,2,0,D,1", "88,99,L,1,1,S,1")) {
            update(payload)
            val image = board.captureToImage()
            assertEquals(4.0, image.width.toDouble() / image.height, 0.06)
            val actual = image.toPixelMap()
            val lit = renderer.pixels(displayed.value)
            for (y in 0 until 16) {
                for (x in 0 until 64) {
                    val color = actual[((x + 0.5) * image.width / 64).toInt(), ((y + 0.5) * image.height / 16).toInt()]
                    assertEquals("$payload pixel $x,$y", if (LitPixel(x, y) in lit) 1f else 0f, color.red, 0f)
                }
            }
        }
    }

    @Test
    fun serveButtonDrawsTheBoardsDirectionAndArrowCount() {
        show()
        val cases =
            listOf(
                Triple("4,4,L,1,0,D,1", "Serving left team, server 1. Change serve", 1 to true),
                Triple("4,4,L,2,0,D,1", "Serving left team, server 2. Change serve", 2 to true),
                Triple("4,4,R,1,0,D,1", "Serving right team, server 1. Change serve", 1 to false),
                Triple("4,4,R,2,0,D,1", "Serving right team, server 2. Change serve", 2 to false),
                Triple("4,4,R,1,0,S,1", "Serving right team. Change serve", 1 to false),
            )
        for ((payload, description, expected) in cases) {
            update(payload)
            key(description).assertIsDisplayed().assertIsEnabled()
            val (count, pointsLeft) = expected
            val pixels = compose.onNodeWithTag("serve-arrows", useUnmergedTree = true).captureToImage().toPixelMap()
            fun lit(
                x: Int,
                y: Int,
            ) = pixels[x, y].red > 0.8f && pixels[x, y].blue < 0.7f // Lemon on Teal
            val middle = pixels.height / 2
            val runs = mutableListOf<IntRange>()
            var start = -1
            for (x in 0 until pixels.width) {
                if (lit(x, middle) && start < 0) start = x
                if (!lit(x, middle) && start >= 0) runs.add(start until x).also { start = -1 }
            }
            if (start >= 0) runs.add(start until pixels.width)
            assertEquals(payload, count, runs.size)
            for (run in runs) {
                fun height(x: Int) = (0 until pixels.height).count { lit(x, it) }
                val first = height(run.first + 1)
                val last = height(run.last - 1)
                assertTrue("$payload direction $first/$last", if (pointsLeft) first < last else first > last)
            }
            save("serve-${payload.substring(4, 7).replace(",", "")}-${payload[10]}", compose.onNodeWithTag("serve"))
        }
    }

    @Test
    fun controlsSendExactCommandsAndScoresComeOnlyFromTheBoard() {
        show()
        repeat(3) { key("Left plus").performClick() }
        key("Right plus").performClick()
        key("Left minus").performClick()
        key("Right minus").performClick()
        compose.onNodeWithTag("serve").performClick()
        text("Switch Court").performClick()
        text("Undo").performClick()
        key("Left score 7").assertExists() // taps never change the displayed score locally
        update("10,5,L,1,0,D,1")
        key("Left score 10").assertExists()
        update("0,0,L,1,0,D,0")
        text("Undo").assertIsNotEnabled()
        key("Left minus").assertIsEnabled() // the board clamps; in-flight corrections are never dropped
        compose.runOnIdle {
            assertEquals(listOf("+L", "+L", "+L", "+R", "-L", "-R", "S", "C", "U"), events)
        }
    }

    @Test
    fun endGameAndResetConfirmAndResumeUsesUndo() {
        show()
        text("End Game").performClick()
        text("Cancel").performClick()
        compose.runOnIdle { assertTrue(events.isEmpty()) }
        text("End Game").performClick()
        text("End game").performClick()
        update("7,5,L,1,1,D,1")
        text("Game over").assertIsDisplayed()
        for (control in listOf("Left plus", "Left minus", "Right plus", "Right minus", "Serving left team, server 1. Change serve")) {
            key(control).assertIsNotEnabled()
        }
        text("Switch Court").assertIsNotEnabled()
        save("landscape-ended", compose.onNodeWithTag("controller"))
        text("Resume").assertIsEnabled().performClick()
        update("7,5,L,1,0,D,1")
        text("End Game").assertIsEnabled()
        text("Reset").performClick()
        save("landscape-reset-dialog", compose.onNodeWithTag("controller"))
        text("Cancel").performClick()
        text("Reset").performClick()
        text("Singles").performClick()
        text("Reset to 0–0").performClick()
        update("0,0,L,1,0,S,1")
        text("Singles · Live").assertIsDisplayed()
        text("Reset").performClick()
        text("Doubles").performClick()
        text("Reset to 0–0").performClick()
        compose.runOnIdle { assertEquals(listOf("E", "U", "0S", "0D"), events) }
    }

    @Test
    fun queuedCommandsDoNotMoveAnyControl() {
        show()
        val anchors =
            listOf(compose.onNodeWithTag("led-preview"), compose.onNodeWithTag("serve"), text("Board")) +
                listOf("Left plus", "Left minus", "Right plus", "Right minus").map(::key) +
                listOf("Undo", "Switch Court", "End Game", "Reset").map(::text)
        val idle = anchors.map { it.getUnclippedBoundsInRoot() }
        for (pending in listOf(1, 3, 12, 32, 0)) {
            compose.runOnIdle { status.value = status.value.copy(pendingWrites = pending) }
            text(if (pending > 0) "Doubles · Sending $pending" else "Doubles · Live").assertIsDisplayed()
            if (pending == 3) save("landscape-sending", compose.onNodeWithTag("controller"))
            assertEquals("pending=$pending", idle, anchors.map { it.getUnclippedBoundsInRoot() })
        }
    }

    @Test
    fun losingSynchronizationRemovesEveryControlUntilResync() {
        show()
        text("Reset").performClick()
        for (phase in ConnectionPhase.values().filter { it != ConnectionPhase.Ready }) {
            compose.runOnIdle { status.value = ControllerStatus(phase, "detail for $phase") }
            text(phase.label).assertIsDisplayed()
            text("detail for $phase").assertIsDisplayed()
            text("Start a new game?").assertDoesNotExist()
            compose.onNodeWithContentDescription("Scoreboard: no live score").assertExists()
            for (control in listOf("Left plus", "Right minus")) key(control).assertDoesNotExist()
            for (label in listOf("Undo", "Switch Court", "End Game", "Reset")) text(label).assertDoesNotExist()
            compose.onNodeWithTag("serve").assertDoesNotExist()
        }
        save("landscape-reconnect", compose.onNodeWithTag("controller"))
        compose.runOnIdle { status.value = ControllerStatus(ConnectionPhase.Ready) }
        key("Left plus").assertIsEnabled()
        compose.runOnIdle { assertTrue(events.isEmpty()) }
    }

    @Test
    fun boardMenuShowsPackagedVersionEvenWhenDisconnected() {
        status.value = ControllerStatus()
        show()
        text("Board").performClick()
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
