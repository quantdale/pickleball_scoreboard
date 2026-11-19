package com.example.pickleballscoreboard.ui

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.pickleballscoreboard.state.GameMode
import com.example.pickleballscoreboard.state.ScoreboardState
import com.example.pickleballscoreboard.state.Side

internal val Paper = Color(0xFFFAF7EE)
internal val Ink = Color(0xFF152E2C)
internal val Teal = Color(0xFF174641)
internal val Chalk = Color(0xFFF6FFF4)
internal val Lemon = Color(0xFFE9F37A)
internal val Aqua = Color(0xFF9EEAD8)
internal val Rim = Color(0xFFDBE1D3)
internal val Muted = Color(0xFF4E6661)

@Composable
internal fun CourtTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme =
            lightColorScheme(
                primary = Teal,
                onPrimary = Chalk,
                background = Paper,
                onBackground = Ink,
                surface = Paper,
                onSurface = Ink,
                onSurfaceVariant = Muted,
                outline = Muted,
            ),
        content = content,
    )
}

internal enum class CourtMark { Ball, Left, Right, Undo, Switch, Board, Menu }

@Composable
internal fun Mark(
    mark: CourtMark,
    tint: Color,
    modifier: Modifier = Modifier.size(24.dp),
) {
    Canvas(modifier) {
        val u = size.minDimension / 24f
        val w = 2 * u

        fun line(
            x: Int,
            y: Int,
            x2: Int,
            y2: Int,
        ) = drawLine(
            tint,
            Offset(x * u, y * u),
            Offset(x2 * u, y2 * u),
            w,
            cap = androidx.compose.ui.graphics.StrokeCap.Round,
        )

        fun arrow(
            left: Boolean,
            y: Int,
        ) {
            val tip = if (left) 4 else 20
            val tail = if (left) 20 else 4
            val bend = if (left) 9 else 15
            line(tip, y, tail, y)
            line(tip, y, bend, y - 5)
            line(tip, y, bend, y + 5)
        }
        when (mark) {
            CourtMark.Left -> {
                arrow(true, 12)
            }

            CourtMark.Right -> {
                arrow(false, 12)
            }

            CourtMark.Switch -> {
                arrow(true, 7)
                arrow(false, 17)
            }

            CourtMark.Undo -> {
                line(4, 8, 9, 3)
                line(4, 8, 9, 13)
                line(4, 8, 14, 8)
                drawArc(tint, -90f, 180f, false, Offset(8 * u, 8 * u), Size(12 * u, 12 * u), style = Stroke(w))
                line(14, 20, 8, 20)
            }

            CourtMark.Board -> {
                drawRoundRect(tint, Offset(7 * u, 2 * u), Size(10 * u, 20 * u), CornerRadius(2 * u), style = Stroke(w))
                line(10, 7, 14, 7)
                line(10, 12, 14, 12)
                line(10, 17, 14, 17)
            }

            CourtMark.Menu -> {
                for (x in listOf(5, 12, 19)) drawCircle(tint, 1.6f * u, Offset(x * u, 12 * u))
            }

            CourtMark.Ball -> {
                drawCircle(tint, 10 * u, Offset(12 * u, 12 * u))
                for ((x, y) in listOf(8 to 7, 14 to 6, 17 to 11, 12 to 12, 6 to 13, 10 to 18, 16 to 17)) {
                    drawCircle(Teal, 1.4f * u, Offset(x * u, y * u))
                }
            }
        }
    }
}

// Keep the entire hit area stationary as the face sinks into its hard edge.
@Composable
internal fun CourtKey(
    label: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    mark: CourtMark? = null,
    face: Color = Chalk,
    edge: Color = Rim,
    textColor: Color = Ink,
    tall: Boolean = false,
) {
    val interaction = remember { MutableInteractionSource() }
    val pressed by interaction.collectIsPressedAsState()
    val travel by animateDpAsState(if (pressed && enabled) 5.dp else 0.dp, tween(85), label = "key travel")
    val shape = RoundedCornerShape(if (tall) 23.dp else 17.dp)
    val front = if (enabled) face else Color(0xFFEBEBE2)
    val side = if (enabled) edge else Color(0xFFDBDFD5)
    val ink = if (enabled) textColor else Muted
    Box(
        modifier.clip(shape).clickable(interaction, indication = null, enabled = enabled, role = Role.Button, onClick = onClick),
    ) {
        Box(Modifier.matchParentSize().background(side, shape))
        Box(
            Modifier
                .fillMaxWidth()
                .padding(bottom = 5.dp)
                .offset(y = travel)
                .heightIn(min = if (tall) 86.dp else 49.dp)
                .background(front, shape)
                .border(1.dp, side, shape)
                .padding(horizontal = 8.dp, vertical = if (tall) 12.dp else 8.dp),
            contentAlignment = Alignment.Center,
        ) {
            if (tall) {
                Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    mark?.let { Mark(it, ink, Modifier.size(28.dp)) }
                    KeyLabel(label, ink, true)
                }
            } else {
                Row(horizontalArrangement = Arrangement.spacedBy(7.dp), verticalAlignment = Alignment.CenterVertically) {
                    mark?.let { Mark(it, ink, Modifier.size(19.dp)) }
                    KeyLabel(label, ink, false)
                }
            }
        }
    }
}

@Composable
private fun KeyLabel(
    label: String,
    ink: Color,
    tall: Boolean,
) {
    Text(label, color = ink, fontSize = if (tall) 16.sp else 14.sp, lineHeight = 19.sp,
        fontWeight = FontWeight.ExtraBold, textAlign = TextAlign.Center)
}

// This is a physical-end readout, not the LED score call (Spec 03 §4).
@Composable
internal fun CourtCard(
    state: ScoreboardState?,
    height: Dp,
    modifier: Modifier = Modifier,
) {
    val shape = RoundedCornerShape(28.dp)
    Column(
        modifier
            .fillMaxWidth()
            .clip(shape)
            .background(Teal)
            .padding(14.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Row(Modifier.fillMaxWidth().padding(horizontal = 4.dp), verticalAlignment = Alignment.CenterVertically) {
            Text(
                if (state?.gameEnded == true) "Final score" else "Match score",
                Modifier.weight(1f),
                color = Chalk,
                fontSize = 19.sp,
                fontWeight = FontWeight.ExtraBold,
            )
            if (state != null) {
                Text(
                    if (state.gameMode == GameMode.SINGLES) "Singles" else "Doubles",
                    Modifier.background(Color(0xFF2F6058), CircleShape).padding(horizontal = 11.dp, vertical = 5.dp),
                    color = Chalk,
                    fontSize = 13.sp,
                    fontWeight = FontWeight.Bold,
                )
            }
        }
        Box(
            Modifier
                .fillMaxWidth()
                .heightIn(min = height)
                .clip(RoundedCornerShape(17.dp))
                .background(Color(0xFF236760)),
            contentAlignment = Alignment.Center,
        ) {
            CourtLines(Modifier.matchParentSize())
            Row(Modifier.fillMaxWidth().padding(horizontal = 10.dp, vertical = 12.dp)) {
                CourtEnd(Side.LEFT, state, Lemon, Modifier.weight(1f))
                Spacer(Modifier.width(12.dp))
                CourtEnd(Side.RIGHT, state, Aqua, Modifier.weight(1f))
            }
        }
    }
}

@Composable
private fun CourtLines(modifier: Modifier) {
    Canvas(modifier) {
        val pad = 9.dp.toPx()
        val rule = Chalk.copy(alpha = .24f)
        val mid = size.width / 2
        val stroke = 1.5.dp.toPx()
        drawRect(rule, Offset(pad, pad), Size(size.width - 2 * pad, size.height - 2 * pad), style = Stroke(stroke))
        drawRect(Teal.copy(alpha = .45f), Offset(mid * .7f, pad), Size(mid * .6f, size.height - pad * 2))
        for (x in listOf(mid * .7f, mid * 1.3f)) drawLine(rule, Offset(x, pad), Offset(x, size.height - pad), stroke)
        drawLine(rule, Offset(pad, size.height / 2), Offset(mid * .7f, size.height / 2), stroke)
        drawLine(rule, Offset(mid * 1.3f, size.height / 2), Offset(size.width - pad, size.height / 2), stroke)
        drawLine(Chalk.copy(alpha = .65f), Offset(mid, pad), Offset(mid, size.height - pad), stroke)
    }
}

@Composable
private fun CourtEnd(
    side: Side,
    state: ScoreboardState?,
    accent: Color,
    modifier: Modifier,
) {
    val name = if (side == Side.LEFT) "Left" else "Right"
    val score = state?.let { if (side == Side.LEFT) it.leftScore else it.rightScore }
    val serving = state?.servingSide == side && !state.gameEnded
    val badge = if (state?.gameMode == GameMode.DOUBLES) "Server ${state.serverNumber}" else "Serving"
    Column(
        modifier.semantics(mergeDescendants = true) {},
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(3.dp),
    ) {
        Text(name, color = accent, fontWeight = FontWeight.ExtraBold, fontSize = 15.sp)
        // Fade real snapshots without interpolating through invented scores.
        AnimatedContent(score, transitionSpec = { fadeIn(tween(130)) togetherWith fadeOut(tween(75)) }, label = "score") { value ->
            val text = value?.toString() ?: "–"
            Text(
                text,
                color = Chalk,
                fontSize =
                    (
                        if (text.length > 6) {
                            17
                        } else if (text.length > 3) {
                            31
                        } else {
                            50
                        }
                    ).sp,
                lineHeight = 54.sp,
                fontFamily = FontFamily.SansSerif,
                fontWeight = FontWeight.Black,
                modifier = if (value == null) Modifier.semantics { contentDescription = "No live score" } else Modifier,
            )
        }
        Box(Modifier.heightIn(min = 30.dp), contentAlignment = Alignment.Center) {
            if (serving) {
                Text(
                    badge,
                    Modifier.background(accent, CircleShape).padding(horizontal = 8.dp, vertical = 4.dp),
                    color = Ink,
                    fontWeight = FontWeight.ExtraBold,
                    fontSize = 13.sp,
                )
            }
        }
    }
}
