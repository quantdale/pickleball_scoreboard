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
                    assertTrue(pixels.all { it.x in 2..13 && it.y in 1..62 && it.y !in 30..35 })
                }
            }
        }
    }
}
