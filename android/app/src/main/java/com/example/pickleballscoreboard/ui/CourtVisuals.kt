package com.example.pickleballscoreboard.ui

import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.interaction.collectIsFocusedAsState
import androidx.compose.foundation.layout.*
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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

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

internal enum class CourtMark { Ball, Left, Right, Switch, Board }

@Composable
internal fun Mark(
    mark: CourtMark,
    tint: Color,
    modifier: Modifier,
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

            CourtMark.Board -> {
                drawRoundRect(tint, Offset(7 * u, 2 * u), Size(10 * u, 20 * u), CornerRadius(2 * u), style = Stroke(w))
                line(10, 7, 14, 7)
                line(10, 12, 14, 12)
                line(10, 17, 14, 17)
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
    rally: Boolean = false,
    fillHeight: Boolean = false, // equal faces in a row where one label wraps
) {
    val interaction = remember { MutableInteractionSource() }
    val pressed by interaction.collectIsPressedAsState()
    val focused by interaction.collectIsFocusedAsState()
    val travel by animateDpAsState(if (pressed && enabled) 5.dp else 0.dp, tween(85), label = "key travel")
    val shape = RoundedCornerShape(if (rally) 18.dp else 15.dp)
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
                .then(if (fillHeight) Modifier.fillMaxHeight() else Modifier)
                .padding(bottom = 5.dp)
                .offset(y = travel)
                .heightIn(min = if (rally) 59.dp else 43.dp)
                .background(front, shape)
                .border(if (focused) 2.dp else 1.dp, if (focused) Teal else side, shape)
                .padding(horizontal = 8.dp, vertical = if (rally) 12.dp else 8.dp),
            contentAlignment = Alignment.Center,
        ) {
            Row(horizontalArrangement = Arrangement.spacedBy(7.dp), verticalAlignment = Alignment.CenterVertically) {
                if (!rally) mark?.let { Mark(it, ink, Modifier.size(19.dp)) }
                if (rally) Box(Modifier.weight(1f).padding(start = 4.dp)) { KeyLabel(label, ink, true) }
                else KeyLabel(label, ink, false)
                if (rally) mark?.let { Mark(it, ink, Modifier.size(22.dp)) }
            }
        }
    }
}

@Composable
private fun KeyLabel(
    label: String,
    ink: Color,
    rally: Boolean,
) {
    Text(
        label,
        color = ink,
        fontSize = if (rally) 16.sp else 14.sp,
        lineHeight = 19.sp,
        fontWeight = FontWeight.ExtraBold,
        textAlign = TextAlign.Center,
    )
}
