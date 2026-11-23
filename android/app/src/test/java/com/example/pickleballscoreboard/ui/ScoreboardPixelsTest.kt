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
        assertEquals(60, cases.length())
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
    fun arrowsSitBetweenFixedScoresAndCountTheServer() {
        fun arrowColumns(state: ScoreboardState) = renderer.pixels(state).filter { it.x in 23..40 }.map { it.x }.toSet()
        assertEquals((29..34).toSet(), arrowColumns(ScoreboardState()))
        assertEquals((25..30).toSet() + (33..38), arrowColumns(ScoreboardState(serverNumber = 2)))
        assertEquals((29..34).toSet(), arrowColumns(ScoreboardState(serverNumber = 2, gameMode = GameMode.SINGLES)))
        val left = renderer.pixels(ScoreboardState(serverNumber = 2, servingSide = Side.LEFT))
        val right = renderer.pixels(ScoreboardState(serverNumber = 2, servingSide = Side.RIGHT))
        // Tip of a left arrow is its first column; the right arrow mirrors each triangle in place.
        assertEquals(2, left.count { it.x == 25 })
        assertEquals(12, right.count { it.x == 25 })
        assertEquals(left.filter { it.x !in 23..40 }, right.filter { it.x !in 23..40 })
    }

    @Test
    fun allScoresStayInTheirOwnRegionInsideADarkBorder() {
        assertEquals(64, renderer.width)
        assertEquals(16, renderer.height)
        for (score in 0..99) {
            val pixels = renderer.pixels(ScoreboardState(leftScore = score, rightScore = 99 - score, servingSide = Side.RIGHT))
            assertTrue(pixels.all { it.y in 1..14 && (it.x in 1..22 || it.x in 25..38 || it.x in 41..62) })
        }
    }
}
