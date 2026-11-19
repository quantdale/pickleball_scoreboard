package com.example.pickleballscoreboard.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.LiveRegionMode
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.liveRegion
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import com.example.pickleballscoreboard.state.GameMode
import com.example.pickleballscoreboard.state.ScoreboardState
import com.example.pickleballscoreboard.state.Side

private val courtColors =
    lightColorScheme(
        primary = Color(0xFF12604C),
        onPrimary = Color.White,
        secondary = Color(0xFF37485B),
        onSecondary = Color.White,
        secondaryContainer = Color(0xFFE2EBEF),
        onSecondaryContainer = Color(0xFF203044),
        surface = Color(0xFFF5F7F4),
        onSurface = Color(0xFF14241D),
        onSurfaceVariant = Color(0xFF45564F),
        outline = Color(0xFF778781),
    )

@Composable
internal fun CourtTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = courtColors, content = content)
}

private enum class Dialog { End, Reset, Maintenance }

// Specs 02/03: only a subscribed, current ESP32 session provides live pixels.
@Composable
internal fun ScoreboardScreen(
    state: ScoreboardState?,
    status: ControllerStatus,
    onDisconnect: () -> Unit,
    onRallyLeft: () -> Unit,
    onRallyRight: () -> Unit,
    onUndo: () -> Unit,
    onSwitchCourts: () -> Unit,
    onEndGame: () -> Unit,
    onReset: (Side, GameMode) -> Unit,
    connectionActions: @Composable () -> Unit = {},
) {
    val live = state.takeIf { status.ready }
    val ready = live != null
    val playing = ready && live?.gameEnded == false
    var menu by remember { mutableStateOf(false) }
    var dialog by remember { mutableStateOf<Dialog?>(null) }
    LaunchedEffect(ready) {
        if (!ready && dialog != Dialog.Maintenance) dialog = null
    }
    Surface(modifier = Modifier.fillMaxSize()) {
        BoxWithConstraints(Modifier.safeDrawingPadding().padding(horizontal = 16.dp)) {
            // Shrink the board on short phones before sacrificing rally target size.
            val heightLimitedWidth = ((maxHeight - 224.dp) / 4).coerceIn(48.dp, 112.dp)
            val boardWidth = (maxWidth * 0.31f).coerceAtMost(heightLimitedWidth)
            val workspaceHeight = (maxHeight - 184.dp).coerceAtLeast(0.dp)
            Column(
                Modifier.fillMaxSize().padding(top = 10.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        "PickleScore",
                        Modifier.weight(1f),
                        style = MaterialTheme.typography.headlineSmall,
                        fontWeight = FontWeight.Bold,
                    )
                    Box {
                        TextButton(onClick = { menu = true }, modifier = Modifier.heightIn(min = 48.dp)) { Text("Board") }
                        DropdownMenu(expanded = menu, onDismissRequest = { menu = false }) {
                            if (status.phase.hasLinkActions) {
                                DropdownMenuItem(text = { Text("Disconnect") }, onClick = {
                                    menu = false
                                    onDisconnect()
                                })
                            }
                            DropdownMenuItem(text = { Text("Maintenance") }, onClick = {
                                menu = false
                                dialog = Dialog.Maintenance
                            })
                        }
                    }
                }
                Surface(
                    color = if (ready) MaterialTheme.colorScheme.secondaryContainer else MaterialTheme.colorScheme.surface,
                    shape = RoundedCornerShape(12.dp),
                ) {
                    Column(
                        Modifier
                            .fillMaxWidth()
                            .semantics { liveRegion = LiveRegionMode.Polite }
                            .padding(horizontal = 12.dp, vertical = 8.dp),
                    ) {
                        Text(status.phase.label, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                        if (ready) {
                            Text(status.transportText, style = MaterialTheme.typography.bodySmall)
                        } else {
                            status.detail?.let { Text(it, style = MaterialTheme.typography.bodySmall) }
                        }
                    }
                }
                Column(
                    Modifier.weight(1f).verticalScroll(rememberScrollState()).padding(bottom = 12.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp),
                ) {
                    if (!ready) connectionActions()
                    Row(
                        Modifier.fillMaxWidth().heightIn(min = if (ready) workspaceHeight else 0.dp),
                        horizontalArrangement = Arrangement.spacedBy(14.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Column(Modifier.width(boardWidth), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                            Text(if (ready) "Live" else "Preview", style = MaterialTheme.typography.labelLarge)
                            ScoreboardPreview(live)
                            Text(
                                when {
                                    live == null -> "No live score"
                                    live.gameEnded -> "Game ended"
                                    live.gameMode == GameMode.SINGLES -> "Singles"
                                    else -> "Doubles"
                                },
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                            Text("Rally winner", style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                            RallyButton("Rally Left", "←", playing, onRallyLeft)
                            RallyButton("Rally Right", "→", playing, onRallyRight, secondary = true)
                            FilledTonalButton(onClick = onUndo, enabled = ready, modifier = actionSize) { Text("Undo") }
                            FilledTonalButton(onClick = onSwitchCourts, enabled = playing, modifier = actionSize) {
                                Text("Switch Courts", textAlign = TextAlign.Center)
                            }
                            OutlinedButton(onClick = { dialog = Dialog.End }, enabled = playing, modifier = actionSize) { Text("End Game") }
                            OutlinedButton(onClick = { dialog = Dialog.Reset }, enabled = ready, modifier = actionSize) { Text("Reset") }
                        }
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
                    text = { Text("The board holds the final score. Undo can resume play.") },
                    confirmButton = {
                        Button(onClick = {
                            if (dialog == Dialog.End) {
                                dialog = null
                                onEndGame()
                            }
                        }) { Text("End game now") }
                    },
                    dismissButton = { TextButton(onClick = { dialog = null }) { Text("Cancel") } },
                )
            }
        }

        Dialog.Reset -> {
            if (ready) {
                ResetDialog(
                    mode = live?.gameMode ?: GameMode.DOUBLES,
                    onDismiss = { dialog = null },
                    onConfirm = { side, mode ->
                        if (dialog == Dialog.Reset) {
                            dialog = null
                            onReset(side, mode)
                        }
                    },
                )
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

private val actionSize = Modifier.fillMaxWidth().heightIn(min = 48.dp)

@Composable
private fun RallyButton(
    label: String,
    arrow: String,
    enabled: Boolean,
    onClick: () -> Unit,
    secondary: Boolean = false,
) {
    Button(
        onClick = onClick,
        enabled = enabled,
        modifier = Modifier.fillMaxWidth().heightIn(min = 64.dp),
        shape = RoundedCornerShape(14.dp),
        colors =
            if (secondary) {
                ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.secondary)
            } else {
                ButtonDefaults.buttonColors()
            },
    ) {
        Text(label, Modifier.weight(1f), style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
        Text(arrow, Modifier.padding(start = 4.dp), style = MaterialTheme.typography.titleMedium)
    }
}

@Composable
internal fun ScoreboardPreview(state: ScoreboardState?) {
    val context = LocalContext.current
    val renderer = remember(context) { DisplayRenderer(context) }
    val pixels = remember(renderer, state) { state?.let(renderer::pixels).orEmpty() }
    val description =
        if (state == null) {
            "Scoreboard: no live score"
        } else {
            "Scoreboard: serving score ${state.servingScore}, opponent ${state.opponentScore}, " +
                "server ${state.serverNumber}, ${state.servingSide.name.lowercase()} serving"
        }
    Canvas(
        Modifier
            .fillMaxWidth()
            .aspectRatio(renderer.aspectRatio)
            .background(Color.Black)
            .testTag("portrait-preview")
            .semantics { contentDescription = description },
    ) {
        with(renderer) { render(pixels) }
    }
}

@Composable
private fun ResetDialog(
    mode: GameMode,
    onDismiss: () -> Unit,
    onConfirm: (Side, GameMode) -> Unit,
) {
    var nextMode by remember { mutableStateOf(mode) }
    var firstSide by remember { mutableStateOf<Side?>(null) }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Reset game?") },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState())) {
                Text("Start a new 0–0 game. Choose mode and first serve.")
                Choice("Doubles", nextMode == GameMode.DOUBLES) { nextMode = GameMode.DOUBLES }
                Choice("Singles", nextMode == GameMode.SINGLES) { nextMode = GameMode.SINGLES }
                Choice("Left serves first", firstSide == Side.LEFT) { firstSide = Side.LEFT }
                Choice("Right serves first", firstSide == Side.RIGHT) { firstSide = Side.RIGHT }
            }
        },
        confirmButton = {
            Button(
                enabled = firstSide != null,
                onClick = { firstSide?.let { onConfirm(it, nextMode) } },
            ) { Text("Reset score") }
        },
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
