package com.example.pickleballscoreboard.ui

import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsFocusedAsState
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.selection.selectableGroup
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.IconButton
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.LiveRegionMode
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.liveRegion
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.pickleballscoreboard.R
import com.example.pickleballscoreboard.ble.BoardBrightness
import com.example.pickleballscoreboard.ble.BoardCommand
import com.example.pickleballscoreboard.ble.LedColor
import com.example.pickleballscoreboard.state.GameMode
import com.example.pickleballscoreboard.state.ScoreboardState
import com.example.pickleballscoreboard.state.Side

private enum class Dialog { End, Reset, Maintenance }

private val LemonEdge = Color(0xFFB1BC4B)
private val AquaEdge = Color(0xFF56AA96)
private val TealEdge = Color(0xFF0D2C29)
private val UnlitBar = Color(0xFFBFC8B8)

// Specs 01–03: landscape controller. Every value shown comes from the subscribed board.
@Composable
internal fun ScoreboardScreen(
    state: ScoreboardState?,
    status: ControllerStatus,
    onDisconnect: () -> Unit,
    onCommand: (BoardCommand) -> Unit,
    brightness: BoardBrightness? = null,
    onBrightness: (Int) -> Unit = {},
    color: LedColor? = null,
    onColor: (LedColor) -> Unit = {},
    connectionActions: @Composable () -> Unit = {},
) {
    val live = state.takeIf { status.ready }
    val playing = live?.gameEnded == false
    var dialog by remember { mutableStateOf<Dialog?>(null) }
    LaunchedEffect(live != null) {
        if (live == null && dialog != Dialog.Maintenance) dialog = null
    }
    Surface(Modifier.fillMaxSize(), color = Paper) {
        Column(
            Modifier.fillMaxSize().safeDrawingPadding().padding(horizontal = 16.dp, vertical = 8.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Header(
                live,
                status,
                color.takeIf { live != null },
                onDisconnect,
                onMaintenance = { dialog = Dialog.Maintenance },
                onEnd = { dialog = Dialog.End },
                onResume = { onCommand(BoardCommand.Undo) },
            )
            if (live == null) {
                Box(Modifier.weight(1f).fillMaxWidth(), contentAlignment = Alignment.Center) {
                    ConnectionPanel(status, connectionActions)
                }
            } else {
                Row(Modifier.weight(1f).fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                    TeamPanel(Side.LEFT, live.leftScore, playing, Lemon, LemonEdge, onCommand, Modifier.weight(1f))
                    ServeButton(live, playing, { onCommand(BoardCommand.NextServe) }, Modifier.weight(0.92f).fillMaxHeight())
                    TeamPanel(Side.RIGHT, live.rightScore, playing, Aqua, AquaEdge, onCommand, Modifier.weight(1f))
                }
                BoxWithConstraints(Modifier.fillMaxWidth()) {
                    // Keep the long label on one line on compact screens without raising the footer.
                    val compact = (brightness != null || color != null) &&
                        maxWidth < 610.dp * LocalDensity.current.fontScale.coerceAtLeast(1f)
                    Row(
                        Modifier.fillMaxWidth().height(IntrinsicSize.Min),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        CourtKey(
                            "Switch Court",
                            { onCommand(BoardCommand.SwitchCourts) },
                            Modifier.weight(if (compact) 2f else 1f).fillMaxHeight(),
                            playing,
                            mark = if (compact) null else CourtMark.Switch,
                            fillHeight = true,
                            compact = compact,
                        )
                        brightness?.let { BrightnessControl(it, onBrightness) }
                        color?.let { ColorControl(it, compact, onColor) }
                        CourtKey(
                            "Reset", { dialog = Dialog.Reset }, Modifier.weight(1f).fillMaxHeight(),
                            fillHeight = true, compact = compact,
                        )
                    }
                }
            }
        }
    }
    when (dialog) {
        Dialog.End -> {
            if (playing) {
                AlertDialog(
                    onDismissRequest = { dialog = null },
                    title = { Text("End this game?") },
                    text = { Text("The board keeps the final score.") },
                    confirmButton = {
                        Button(onClick = {
                            dialog = null
                            onCommand(BoardCommand.EndGame)
                        }) { Text("End game") }
                    },
                    dismissButton = { TextButton(onClick = { dialog = null }) { Text("Cancel") } },
                )
            }
        }

        Dialog.Reset -> {
            if (live != null) {
                ResetDialog(live.gameMode, onDismiss = { dialog = null }) { mode ->
                    dialog = null
                    onCommand(if (mode == GameMode.SINGLES) BoardCommand.NewSinglesGame else BoardCommand.NewDoublesGame)
                }
            }
        }

        Dialog.Maintenance -> {
            AlertDialog(
                onDismissRequest = { dialog = null },
                title = { Text("Board maintenance") },
                text = {
                    Text(
                        "Disconnect this controller before using the authorized PickleScore maintenance tool. Updates are not initiated from this app.",
                    )
                },
                confirmButton = { TextButton(onClick = { dialog = null }) { Text("Close") } },
            )
        }

        null -> {
            Unit
        }
    }
}

@Composable
private fun Header(
    live: ScoreboardState?,
    status: ControllerStatus,
    color: LedColor?,
    onDisconnect: () -> Unit,
    onMaintenance: () -> Unit,
    onEnd: () -> Unit,
    onResume: () -> Unit,
) {
    var menu by remember { mutableStateOf(false) }
    // Spec 01 §6: a second Undo before the board reports Resume could undo a real input.
    var resumeSent by remember(live) { mutableStateOf(false) }
    BoxWithConstraints(Modifier.fillMaxWidth()) {
        // Give scoring status and the LED preview priority over the wordmark.
        val showTitle = maxWidth >= 590.dp * LocalDensity.current.fontScale.coerceAtLeast(1f)
        val appName = stringResource(R.string.app_name)
        Row(Modifier.fillMaxWidth().height(44.dp), verticalAlignment = Alignment.CenterVertically) {
            Image(
                painterResource(R.drawable.mt_pickle_park_mark),
                contentDescription = if (showTitle) null else appName,
                modifier = Modifier.width(52.dp).height(38.dp),
            )
            if (showTitle) {
                Spacer(Modifier.width(10.dp))
                Text(appName, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Black, maxLines = 1)
            }
            Box(Modifier.weight(1f).fillMaxHeight().padding(horizontal = 8.dp), contentAlignment = Alignment.Center) {
                Box(Modifier.background(Teal, RoundedCornerShape(8.dp)).padding(3.dp)) {
                    ScoreboardPreview(live, Modifier.widthIn(max = 128.dp), color ?: LedColor.White)
                }
            }
            StatusChip(live, status)
            Box {
                TextButton(onClick = { menu = true }, modifier = Modifier.heightIn(min = 48.dp)) {
                    Text("Board", fontWeight = FontWeight.Bold)
                }
                DropdownMenu(expanded = menu, onDismissRequest = { menu = false }) {
                    Text(
                        stringResource(R.string.controller_version),
                        Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                        style = MaterialTheme.typography.labelLarge,
                        color = Muted,
                    )
                    if (live != null) {
                        HorizontalDivider(color = Rim)
                        if (live.gameEnded) {
                            DropdownMenuItem(text = { Text("Resume") }, enabled = live.canUndo && !resumeSent, onClick = {
                                menu = false
                                resumeSent = true
                                onResume()
                            })
                        } else {
                            DropdownMenuItem(text = { Text("End Game") }, onClick = {
                                menu = false
                                onEnd()
                            })
                        }
                    }
                    if (status.phase.hasLinkActions) {
                        DropdownMenuItem(text = { Text("Disconnect") }, onClick = {
                            menu = false
                            onDisconnect()
                        })
                        DropdownMenuItem(text = { Text("Maintenance") }, onClick = {
                            menu = false
                            onMaintenance()
                        })
                    }
                }
            }
        }
    }
}

// Spec 02 §6: queued or sending commands are shown apart from the authoritative score.
@Composable
private fun StatusChip(
    live: ScoreboardState?,
    status: ControllerStatus,
) {
    val text =
        when {
            live == null -> status.phase.label
            status.pendingWrites > 0 -> "Sending ${status.pendingWrites}"
            else -> "Live"
        }
    val mode = live?.let { if (it.gameMode == GameMode.SINGLES) "Singles" else "Doubles" }
    val style = MaterialTheme.typography.labelLarge.copy(fontFeatureSettings = "tnum", fontWeight = FontWeight.Bold)
    Surface(
        color = if (live != null) Color(0xFFE1F1D9) else Color(0xFFEDEADE),
        shape = RoundedCornerShape(50),
        modifier = Modifier.semantics { liveRegion = LiveRegionMode.Polite },
    ) {
        Box(Modifier.padding(horizontal = 14.dp, vertical = 7.dp), contentAlignment = Alignment.Center) {
            // Reserve the widest message at the current font scale so the header never shifts
            // while commands are queued; the bounded GATT queue has at most two digits.
            Box(Modifier.alpha(0f).clearAndSetSemantics { }) {
                Text("Doubles · Sending 88", style = style, maxLines = 1)
                Text(ConnectionPhase.ReconnectRequired.label, style = style, maxLines = 1)
            }
            Text(
                listOfNotNull(mode, text).joinToString(" · "),
                Modifier.matchParentSize(),
                style = style,
                textAlign = TextAlign.Center,
                maxLines = 1,
            )
        }
    }
}

// Spec 02 §5, Spec 03 §2a: the meter shows only the board's reported level; taps ask the
// board to step it. Brightness is a display setting, apart from scoring and Undo.
@Composable
private fun BrightnessControl(
    brightness: BoardBrightness,
    onStep: (Int) -> Unit,
) {
    Row(
        Modifier.background(Chalk, RoundedCornerShape(15.dp)).border(1.dp, Rim, RoundedCornerShape(15.dp)),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        StepButton("Dimmer", plus = false, enabled = brightness.level > 1) { onStep(-1) }
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Text("Brightness", style = MaterialTheme.typography.labelSmall, color = Muted, maxLines = 1)
            BrightnessMeter(brightness)
        }
        StepButton("Brighter", plus = true, enabled = brightness.level < brightness.maxLevel) { onStep(1) }
    }
}

@Composable
private fun StepButton(
    description: String,
    plus: Boolean,
    enabled: Boolean,
    onClick: () -> Unit,
) {
    val haptics = LocalHapticFeedback.current
    IconButton(
        onClick = {
            haptics.performHapticFeedback(HapticFeedbackType.TextHandleMove)
            onClick()
        },
        enabled = enabled,
        modifier = Modifier.size(48.dp).semantics { contentDescription = description },
    ) { KeyGlyph(plus, if (enabled) Ink else Muted.copy(alpha = 0.38f), Modifier.size(18.dp)) }
}

// Rising bars, one per level the board allows; lit up to the reported level.
@Composable
private fun BrightnessMeter(
    brightness: BoardBrightness,
) {
    val count = brightness.maxLevel
    Canvas(
        Modifier
            .size(width = 44.dp, height = 18.dp)
            .testTag("brightness-meter")
            .semantics {
                contentDescription = "Brightness ${brightness.level} of $count"
                liveRegion = LiveRegionMode.Polite
            },
    ) {
        val gap = size.width / 12f
        val width = (size.width - gap * (count - 1)) / count
        for (index in 0 until count) {
            val height = size.height * (0.35f + 0.65f * (index + 1) / count)
            drawRoundRect(
                if (index < brightness.level) Teal else UnlitBar,
                Offset(index * (width + gap), size.height - height),
                Size(width, height),
                CornerRadius(2.dp.toPx()),
            )
        }
    }
}

// Spec 02 §5a, Spec 03 §3: one color for every lit LED. The ring marks the board's
// reported color; a tap only asks the board to change it.
@Composable
private fun ColorControl(
    reported: LedColor,
    compact: Boolean,
    onSelect: (LedColor) -> Unit,
) {
    var open by remember { mutableStateOf(false) }
    val haptics = LocalHapticFeedback.current
    Box {
        OutlinedButton(
            onClick = { open = true },
            modifier = Modifier.heightIn(min = 48.dp).semantics { stateDescription = reported.label },
            contentPadding = PaddingValues(horizontal = if (compact) 8.dp else 12.dp),
        ) {
            Box(
                Modifier.size(if (compact) 14.dp else 18.dp)
                    .background(Color(reported.argb), CircleShape).border(1.dp, Muted, CircleShape),
            )
            Spacer(Modifier.width(6.dp))
            Text("LED Color", style = if (compact) MaterialTheme.typography.labelMedium else MaterialTheme.typography.labelLarge, maxLines = 1)
        }
        DropdownMenu(expanded = open, onDismissRequest = { open = false }) {
            Row(Modifier.padding(horizontal = 4.dp).selectableGroup().testTag("led-colors")) {
                for (choice in LedColor.entries) {
                    val selected = choice == reported
                    Box(
                        Modifier
                            .size(48.dp)
                            .selectable(selected, role = Role.RadioButton) {
                                open = false
                                haptics.performHapticFeedback(HapticFeedbackType.TextHandleMove)
                                onSelect(choice)
                            }.semantics { contentDescription = choice.label },
                        contentAlignment = Alignment.Center,
                    ) {
                        Box(
                            Modifier
                                .size(34.dp)
                                .border(if (selected) 3.dp else 0.dp, if (selected) Ink else Color.Transparent, CircleShape)
                                .padding(5.dp)
                                .background(Color(choice.argb), CircleShape)
                                .border(1.dp, Rim, CircleShape),
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun ConnectionPanel(
    status: ControllerStatus,
    connectionActions: @Composable () -> Unit,
) {
    Column(
        Modifier.widthIn(max = 460.dp).fillMaxWidth().verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        // The header chip names the phase; the panel adds only detail and actions.
        status.detail?.let { Text(it, style = MaterialTheme.typography.titleMedium) }
        connectionActions()
    }
}

// Owner sketch: + above, the team's score, − below, on that team's physical end.
@Composable
private fun TeamPanel(
    side: Side,
    score: Int,
    enabled: Boolean,
    face: Color,
    edge: Color,
    onCommand: (BoardCommand) -> Unit,
    modifier: Modifier,
) {
    val name = if (side == Side.LEFT) "Left" else "Right"
    BoxWithConstraints(modifier.fillMaxHeight()) {
        val keyHeight = (maxHeight * 0.27f).coerceIn(52.dp, 96.dp)
        Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            PressKey(
                "$name plus",
                { onCommand(if (side == Side.LEFT) BoardCommand.LeftPlus else BoardCommand.RightPlus) },
                Modifier.fillMaxWidth().height(keyHeight),
                enabled,
                face,
                edge,
            ) { ink -> KeyGlyph(plus = true, ink, Modifier.size(34.dp)) }
            ScoreReadout(score, name, Modifier.weight(1f).fillMaxWidth())
            PressKey(
                "$name minus",
                { onCommand(if (side == Side.LEFT) BoardCommand.LeftMinus else BoardCommand.RightMinus) },
                Modifier.fillMaxWidth().height(keyHeight),
                enabled,
                Chalk,
                Rim,
            ) { ink -> KeyGlyph(plus = false, ink, Modifier.size(34.dp)) }
        }
    }
}

// Vector strokes keep + and − the same weight; the font's minus is visibly lighter.
@Composable
private fun KeyGlyph(
    plus: Boolean,
    ink: Color,
    modifier: Modifier,
) {
    Canvas(modifier) {
        val stroke = size.width * 0.2f
        val mid = size.width / 2
        val end = size.width - stroke / 2
        drawLine(ink, Offset(stroke / 2, mid), Offset(end, mid), stroke, StrokeCap.Round)
        if (plus) drawLine(ink, Offset(mid, stroke / 2), Offset(mid, end), stroke, StrokeCap.Round)
    }
}

@Composable
private fun ScoreReadout(
    score: Int,
    name: String,
    modifier: Modifier,
) {
    BoxWithConstraints(modifier.semantics { contentDescription = "$name score $score" }, contentAlignment = Alignment.Center) {
        // Fixed visual size: the readout is a display, not body text, so ignore font scale.
        val size = minOf(maxHeight * 0.9f, maxWidth / 1.3f)
        val fontSize = with(LocalDensity.current) { size.toSp() }
        Text(
            score.toString(),
            Modifier.clearAndSetSemantics { }.testTag("$name score"),
            color = Ink,
            fontSize = fontSize,
            lineHeight = fontSize,
            fontWeight = FontWeight.Black,
            style = MaterialTheme.typography.displayLarge.copy(fontFeatureSettings = "tnum"),
            maxLines = 1,
        )
    }
}

// Spec 01 §5: one tap advances L1 → L2 → R1 → R2; arrows match the board's count and direction.
@Composable
private fun ServeButton(
    live: ScoreboardState,
    enabled: Boolean,
    onClick: () -> Unit,
    modifier: Modifier,
) {
    val side = if (live.servingSide == Side.LEFT) "left" else "right"
    val server = if (live.gameMode == GameMode.DOUBLES) ", server ${live.serverNumber}" else ""
    PressKey("Serving $side team$server. Change serve", onClick, modifier.testTag("serve"), enabled, Teal, TealEdge, Chalk) { ink ->
        Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(10.dp)) {
            ServeArrows(live.servingSide, live.arrowCount, if (enabled) Lemon else ink, Modifier.fillMaxWidth().weight(1f, fill = false))
            if (live.gameEnded) Text("Game over", color = Ink, fontWeight = FontWeight.Black, fontSize = 20.sp)
        }
    }
}

// Spec 03 §4b proportions: 1:2 solid triangles with a gap of one third of their width.
@Composable
private fun ServeArrows(
    side: Side,
    count: Int,
    color: Color,
    modifier: Modifier,
) {
    Canvas(modifier.aspectRatio(1.6f).testTag("serve-arrows")) {
        val height = minOf(size.height * 0.8f, size.width / 1.4f)
        val width = height / 2
        val gap = width / 3
        val total = count * width + (count - 1) * gap
        val top = (size.height - height) / 2
        repeat(count) { index ->
            val left = (size.width - total) / 2 + index * (width + gap)
            val tip = if (side == Side.LEFT) left else left + width
            val base = if (side == Side.LEFT) left + width else left
            drawPath(
                Path().apply {
                    moveTo(tip, top + height / 2)
                    lineTo(base, top)
                    lineTo(base, top + height)
                    close()
                },
                color,
            )
        }
    }
}

// Keep the entire hit area stationary while the face sinks into its edge.
@Composable
private fun PressKey(
    description: String,
    onClick: () -> Unit,
    modifier: Modifier,
    enabled: Boolean,
    face: Color,
    edge: Color,
    onFace: Color = Ink,
    content: @Composable (ink: Color) -> Unit,
) {
    val interaction = remember { MutableInteractionSource() }
    val pressed by interaction.collectIsPressedAsState()
    val focused by interaction.collectIsFocusedAsState()
    val travel by animateDpAsState(if (pressed && enabled) 6.dp else 0.dp, tween(70), label = "key travel")
    val haptics = LocalHapticFeedback.current
    val shape = RoundedCornerShape(20.dp)
    val front = if (enabled) face else Color(0xFFEBEBE2)
    val side = if (enabled) edge else Color(0xFFDBDFD5)
    val ink = if (enabled) onFace else Muted
    Box(
        modifier
            .clip(shape)
            .clickable(interaction, indication = null, enabled = enabled, role = Role.Button) {
                haptics.performHapticFeedback(HapticFeedbackType.TextHandleMove)
                onClick()
            }.semantics { contentDescription = description },
    ) {
        Box(Modifier.matchParentSize().background(side, shape))
        Box(
            Modifier
                .fillMaxSize()
                .padding(bottom = 6.dp)
                .offset(y = travel)
                .background(front, shape)
                .border(if (focused) 3.dp else 1.dp, if (focused) Ink else side, shape)
                .padding(8.dp),
            contentAlignment = Alignment.Center,
        ) { content(ink) }
    }
}

@Composable
internal fun ScoreboardPreview(
    state: ScoreboardState?,
    modifier: Modifier = Modifier,
    color: LedColor = LedColor.White,
) {
    val context = LocalContext.current
    val renderer = remember(context) { DisplayRenderer(context) }
    val pixels = remember(renderer, state) { state?.let(renderer::pixels).orEmpty() }
    val description =
        if (state == null) {
            "Scoreboard: no live score"
        } else {
            val server = if (state.gameMode == GameMode.DOUBLES) ", server ${state.serverNumber}" else ""
            "Scoreboard: left ${state.leftScore}, right ${state.rightScore}, " +
                "${state.servingSide.name.lowercase()} serving$server"
        }
    Canvas(
        modifier
            .aspectRatio(renderer.aspectRatio)
            .background(Color.Black)
            .testTag("led-preview")
            .semantics { contentDescription = description },
    ) {
        with(renderer) { render(pixels, Color(color.argb)) }
    }
}

@Composable
private fun ResetDialog(
    mode: GameMode,
    onDismiss: () -> Unit,
    onConfirm: (GameMode) -> Unit,
) {
    var nextMode by remember { mutableStateOf(mode) }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Start a new game?") },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState())) {
                Choice("Doubles", nextMode == GameMode.DOUBLES) { nextMode = GameMode.DOUBLES }
                Choice("Singles", nextMode == GameMode.SINGLES) { nextMode = GameMode.SINGLES }
            }
        },
        confirmButton = { Button(onClick = { onConfirm(nextMode) }) { Text("Reset to 0–0") } },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } },
    )
}

@Composable
private fun Choice(
    label: String,
    selected: Boolean,
    select: () -> Unit,
) {
    Row(
        Modifier.fillMaxWidth().heightIn(min = 48.dp).selectable(selected, role = Role.RadioButton, onClick = select),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        RadioButton(selected = selected, onClick = null)
        Text(label)
    }
}
