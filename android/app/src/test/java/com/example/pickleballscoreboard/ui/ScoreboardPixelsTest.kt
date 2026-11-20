package com.example.pickleballscoreboard.ui

import com.example.pickleballscoreboard.state.GameMode
import com.example.pickleballscoreboard.state.ScoreboardState
import com.example.pickleballscoreboard.state.Side
import org.json.JSONArray
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class ScoreboardPixelsTest {
    private fun resource(name: String) =
        javaClass.classLoader!!
            .getResourceAsStream(name)!!
            .bufferedReader()
            .use { it.readText() }

    private val renderer = ScoreboardPixels(resource("font_5x7.json"), resource("arrows.json"), resource("layout.json"))

    @Test
    fun everyPixelMatchesSharedFixturesAlsoVerifiedByActualCppRenderer() {
        val cases = JSONArray(resource("scoreboard_snapshots.json"))
        assertEquals(72, cases.length())
        for (i in 0 until cases.length()) {
            val case = cases.getJSONObject(i)
            val state =
                ScoreboardState(
                    leftScore = case.getInt("leftScore"),
                    rightScore = case.getInt("rightScore"),
                    servingSide = if (case.getString("servingSide") == "L") Side.LEFT else Side.RIGHT,
                    serverNumber = case.getInt("serverNumber"),
                    gameEnded = case.getBoolean("gameEnded"),
                    gameMode = if (case.getString("gameMode") == "S") GameMode.SINGLES else GameMode.DOUBLES,
                )
            val expected =
                buildSet {
                    val rows = case.getJSONArray("rows")
                    for (y in 0 until rows.length()) {
                        rows.getString(y).forEachIndexed { x, bit ->
                            if (bit == '1') add(LitPixel(x, y))
                        }
                    }
                }
            assertEquals(case.getString("name"), expected, renderer.pixels(state))
        }
    }

    @Test
    fun startupZerosOccupyTwoFullHeightBandsWithAnUnlitGap() {
        val rows = renderer.pixels(ScoreboardState()).map { it.y }.toSet()
        assertEquals((1..14).toSet() + (17..30), rows.filter { it < 32 }.toSet())
    }

    @Test
    fun singlesUsesOnlyCenteredScoreAndArrowBandsRegardlessOfServerField() {
        val singles = ScoreboardState(gameMode = GameMode.SINGLES)
        val left = renderer.pixels(singles)
        val right = renderer.pixels(singles.copy(servingSide = Side.RIGHT))
        val bands = (8..21).toSet() + (27..40) + (46..55)
        assertEquals(bands, left.map { it.y }.toSet())
        assertEquals(left, renderer.pixels(singles.copy(serverNumber = 1)))
        assertEquals(right, left.map { if (it.y >= 46) it.copy(x = 15 - it.x) else it }.toSet())
        assertEquals(36, left.count { it.y >= 46 })
        assertEquals(3 to 12, left.minOf { it.x } to left.maxOf { it.x })
    }

    @Test
    fun allScoresAndIndicatorsRemainInsidePanelsWithBlackSeam() {
        assertEquals(16, renderer.width)
        assertEquals(64, renderer.height)
        for (score in 0..99) {
            for (side in Side.values()) {
                for (server in 1..2) {
                    val pixels =
                        renderer.pixels(
                            ScoreboardState(
                                leftScore = score,
                                rightScore = 99 - score,
                                servingSide = side,
                                serverNumber = server,
                            ),
                        )
                    assertTrue(pixels.isNotEmpty())
                    assertTrue(pixels.all { it.x in 2..13 && it.y in 1..62 && it.y !in 15..16 && it.y !in 31..35 })
                    val singles = ScoreboardState(score, 99 - score, side, server, gameMode = GameMode.SINGLES)
                    assertTrue(renderer.pixels(singles).all { it.x in 2..13 && (it.y in 8..21 || it.y in 27..40 || it.y in 46..55) })
                }
            }
        }
    }
}
