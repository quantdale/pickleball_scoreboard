package com.example.pickleballscoreboard.ui

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
import androidx.compose.ui.semantics.SemanticsActions
import androidx.compose.ui.semantics.SemanticsProperties
import androidx.compose.ui.test.*
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.text.TextLayoutResult
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.dp
import androidx.test.platform.app.InstrumentationRegistry
import com.example.pickleballscoreboard.R
import com.example.pickleballscoreboard.ble.BoardBrightness
import com.example.pickleballscoreboard.ble.LedColor
import com.example.pickleballscoreboard.ble.parseStatePayload
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import java.io.File

// Emulator verification only: no simulated connection in the production APK.
class ScoreboardScreenTest {
    @get:Rule
    val compose = createAndroidComposeRule<LandscapeTestActivity>()
    private val instrumentation get() = InstrumentationRegistry.getInstrumentation()
    private val context get() = instrumentation.targetContext
    private val displayed = mutableStateOf(checkNotNull(parseStatePayload("7,5,L,1,0,D,1")))
    private val status = mutableStateOf(ControllerStatus(ConnectionPhase.Ready))
    private val brightness = mutableStateOf<BoardBrightness?>(BoardBrightness(1, 4))
    private val ledColor = mutableStateOf<LedColor?>(LedColor.White)
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
                            brightness = brightness.value,
                            onBrightness = { events.add(if (it > 0) "brighter" else "dimmer") },
                            color = ledColor.value,
                            onColor = { events.add(it.wire) },
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
            for (label in listOf("Undo", "End Game", "Resume")) text(label).assertDoesNotExist()
            val footer = listOf(text("Switch Court"), key("Dimmer"), key("Brighter"), text("LED Color"), text("Reset"))
            var previousRight = root.left
            for (control in footer) {
                val bounds = control.assertIsDisplayed().getUnclippedBoundsInRoot()
                assertTrue("below the scores", bounds.top >= leftMinus.bottom)
                assertTrue("left to right, no overlap", bounds.left >= previousRight)
                assertTrue("touch target", bounds.bottom - bounds.top >= 47.5.dp && bounds.right - bounds.left >= 47.5.dp)
                assertTrue("on screen", bounds.bottom <= root.bottom && bounds.right <= root.right)
                previousRight = bounds.right
            }
            for (label in listOf("Switch Court", "LED Color", "Reset")) {
                val labelLayouts = mutableListOf<TextLayoutResult>()
                compose.onNodeWithText(label, useUnmergedTree = true)
                    .performSemanticsAction(SemanticsActions.GetTextLayoutResult) { it(labelLayouts) }
                val layout = labelLayouts.single()
                assertEquals("$label stays on one line at font scale $scale", 1, layout.lineCount)
                val lineWidth = layout.getLineRight(0) - layout.getLineLeft(0)
                assertTrue("$label fits at font scale $scale ($lineWidth vs ${layout.size.width})", lineWidth <= layout.size.width + 1f)
            }
            val appName = context.getString(R.string.app_name)
            compose.onNode(hasText(appName) or hasContentDescription(appName)).assertIsDisplayed()
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
        key("Left score 7").assertExists() // taps never change the displayed score locally
        update("10,5,L,1,0,D,1")
        key("Left score 10").assertExists()
        update("0,0,L,1,0,D,0")
        key("Left minus").assertIsEnabled() // the board clamps; in-flight corrections are never dropped
        compose.runOnIdle {
            assertEquals(listOf("+L", "+L", "+L", "+R", "-L", "-R", "S", "C"), events)
        }
    }

    @Test
    fun endGameAndResumeAreSecondaryAndResetStillConfirms() {
        show()
        text("End Game").assertDoesNotExist()
        text("Board").performClick()
        text("End Game").performClick()
        text("Cancel").performClick()
        compose.runOnIdle { assertTrue(events.isEmpty()) }
        text("Board").performClick()
        text("End Game").performClick()
        text("End game").performClick()
        update("7,5,L,1,1,D,1")
        text("Game over").assertIsDisplayed()
        for (control in listOf("Left plus", "Left minus", "Right plus", "Right minus", "Serving left team, server 1. Change serve")) {
            key(control).assertIsNotEnabled()
        }
        text("Switch Court").assertIsNotEnabled()
        save("landscape-ended", compose.onNodeWithTag("controller"))
        text("Resume").assertDoesNotExist()
        text("Board").performClick()
        text("Resume").assertIsEnabled().performClick()
        text("Board").performClick()
        text("Resume").assertIsNotEnabled() // a second tap would undo a real input
        update("7,5,L,1,0,D,1")
        text("End Game").assertIsEnabled().performClick()
        text("Cancel").performClick()
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
                listOf("Switch Court", "LED Color", "Reset").map(::text)
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
        text("Start a new game?").assertIsDisplayed()
        for (phase in ConnectionPhase.values().filter { it != ConnectionPhase.Ready }) {
            compose.runOnIdle { status.value = ControllerStatus(phase, "detail for $phase") }
            text(phase.label).assertIsDisplayed()
            text("detail for $phase").assertIsDisplayed()
            text("Start a new game?").assertDoesNotExist()
            compose.onNodeWithContentDescription("Scoreboard: no live score").assertExists()
            for (control in listOf("Left plus", "Right minus", "Brighter", "White")) key(control).assertDoesNotExist()
            for (label in listOf("Switch Court", "LED Color", "Reset")) text(label).assertDoesNotExist()
            compose.onNodeWithTag("serve").assertDoesNotExist()
        }
        save("landscape-reconnect", compose.onNodeWithTag("controller"))
        compose.runOnIdle { status.value = ControllerStatus(ConnectionPhase.Ready) }
        key("Left plus").assertIsEnabled()
        text("LED Color").performClick()
        key("White").assertIsDisplayed()
        compose.runOnIdle { status.value = ControllerStatus(ConnectionPhase.ReconnectRequired) }
        key("White").assertDoesNotExist() // an open palette closes on loss of synchronization
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

    @Test
    fun brightnessIsInlineAndShowsOnlyTheBoardsLevel() {
        show()
        val anchors =
            listOf(compose.onNodeWithTag("led-preview"), compose.onNodeWithTag("serve"), text("Board")) +
                listOf("Left plus", "Left minus", "Right plus", "Right minus").map(::key) +
                listOf("Switch Court", "LED Color", "Reset").map(::text)
        val idle = anchors.map { it.getUnclippedBoundsInRoot() }
        text("Brightness").assertIsDisplayed()
        key("Brightness 1 of 4").assertIsDisplayed()
        key("Dimmer").assertIsNotEnabled()
        repeat(3) { key("Brighter").assertIsEnabled().performClick() }
        key("Brightness 1 of 4").assertIsDisplayed() // taps never move the meter locally
        compose.runOnIdle { brightness.value = BoardBrightness(3, 4) }
        key("Brightness 3 of 4").assertIsDisplayed()
        save("landscape-brightness", compose.onNodeWithTag("controller"))
        compose.runOnIdle { brightness.value = BoardBrightness(4, 4) }
        key("Brighter").assertIsNotEnabled()
        key("Dimmer").assertIsEnabled().performClick()
        for (control in listOf("Dimmer", "Brighter")) {
            val bounds = key(control).getUnclippedBoundsInRoot()
            assertTrue("$control touch target", bounds.bottom - bounds.top >= 47.5.dp && bounds.right - bounds.left >= 47.5.dp)
        }
        // Teal bar (red ~0.09, green ~0.27) vs unlit (red ~0.75); black or blank captures fail both.
        fun bars(): List<Boolean?> {
            val meter = compose.onNodeWithTag("brightness-meter").assertIsDisplayed().captureToImage().toPixelMap()
            return (0 until 4).map {
                val color = meter[(it * 12 + 4) * meter.width / 44, meter.height - 2]
                when {
                    color.red < 0.3f && color.green > 0.15f -> true
                    color.red > 0.7f -> false
                    else -> null
                }
            }
        }
        assertEquals("4 of 4", listOf(true, true, true, true), bars())
        compose.runOnIdle { brightness.value = BoardBrightness(2, 4) }
        assertEquals("2 of 4", listOf(true, true, false, false), bars())
        compose.runOnIdle { assertEquals(listOf("brighter", "brighter", "brighter", "dimmer"), events) }
        assertEquals(idle, anchors.map { it.getUnclippedBoundsInRoot() })
    }

    @Test
    fun onlyReportedSettingsAreShownAndBoardMenuKeepsLinkActions() {
        ledColor.value = null
        show()
        key("Brighter").assertIsDisplayed()
        text("LED Color").assertDoesNotExist()
        compose.runOnIdle { brightness.value = null }
        text("Brightness").assertDoesNotExist()
        key("Brighter").assertDoesNotExist()
        text("Switch Court").assertIsDisplayed()
        text("Reset").assertIsDisplayed()
        text("Board").performClick()
        text("Disconnect").assertIsDisplayed()
        text("Maintenance").assertIsDisplayed()
        text("Brightness").assertDoesNotExist()
        text("LED Color").assertDoesNotExist()
        compose.runOnIdle { assertTrue(events.isEmpty()) }
    }

    @Test
    fun colorSwatchesShowOnlyTheBoardsColorAndTintThePreview() {
        show()
        text("LED Color").assertIsDisplayed()
        // The LED preview shows lit pixels in the board's color: pure red, never white.
        compose.runOnIdle { ledColor.value = LedColor.Red }
        update("0,0,L,1,0,D,0")
        fun litColors(): Set<Triple<Float, Float, Float>> {
            val image = compose.onNodeWithTag("led-preview").captureToImage().toPixelMap()
            val lit = mutableSetOf<Triple<Float, Float, Float>>()
            for (y in 0 until 16) {
                for (x in 0 until 64) {
                    val c = image[((x + 0.5) * image.width / 64).toInt(), ((y + 0.5) * image.height / 16).toInt()]
                    if (c.red + c.green + c.blue > 0f) lit.add(Triple(c.red, c.green, c.blue))
                }
            }
            return lit
        }
        assertEquals(setOf(Triple(1f, 0f, 0f)), litColors())
        compose.runOnIdle { ledColor.value = LedColor.Cyan }
        assertEquals(setOf(Triple(0f, 1f, 1f)), litColors())
        compose.runOnIdle { ledColor.value = LedColor.White }
        text("LED Color").performClick()
        val swatches = compose.onAllNodes(SemanticsMatcher.expectValue(SemanticsProperties.Role, Role.RadioButton))
        swatches.assertCountEquals(LedColor.entries.size)
        val root = compose.onNodeWithTag("controller").getUnclippedBoundsInRoot()
        for (choice in LedColor.entries) {
            val bounds = key(choice.label).assertIsDisplayed().getUnclippedBoundsInRoot()
            assertTrue("${choice.label} touch target", bounds.bottom - bounds.top >= 47.5.dp && bounds.right - bounds.left >= 47.5.dp)
            assertTrue("${choice.label} on screen", bounds.right <= root.right && bounds.bottom <= root.bottom)
        }
        key("White").assertIsSelected()
        key("Red").assertIsNotSelected().performClick()
        key("Red").assertDoesNotExist() // closes immediately; a tap does not change the reported color
        text("LED Color").assert(SemanticsMatcher.expectValue(SemanticsProperties.StateDescription, "White")).performClick()
        key("White").assertIsSelected()
        compose.runOnIdle { ledColor.value = LedColor.Red }
        key("Red").assertIsSelected()
        key("White").assertIsNotSelected()
        key("Green").performClick()
        compose.runOnIdle { assertEquals(listOf("KR", "KG"), events) }
    }

    @Test
    fun colorPaletteFitsWithLargeText() {
        fontScale.floatValue = 1.35f
        ledColor.value = LedColor.Red
        brightness.value = BoardBrightness(2, 4)
        show()
        val root = compose.onNodeWithTag("controller").getUnclippedBoundsInRoot()
        text("LED Color").performClick()
        for (control in LedColor.entries.map { it.label }) {
            val bounds = key(control).assertIsDisplayed().getUnclippedBoundsInRoot()
            assertTrue("$control on screen", bounds.right <= root.right && bounds.bottom <= root.bottom)
        }
        saveScreen("landscape-led-palette-font-135")
    }

    // The palette is a popup window; capture the whole display to show it in place.
    private fun saveScreen(name: String) {
        compose.waitForIdle()
        val bitmap = checkNotNull(instrumentation.uiAutomation.takeScreenshot())
        File(context.getExternalFilesDir(null), "$name.png").outputStream().use {
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, it)
        }
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
