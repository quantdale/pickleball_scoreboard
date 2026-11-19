package com.example.pickleballscoreboard.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.IconButton
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.sp
import androidx.compose.foundation.background
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
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

private enum class Dialog { End, Reset, Preview, Maintenance }

// Specs 02/03: the synchronized board is authoritative; buttons only send commands.
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
    val playing = live?.gameEnded == false
    var menu by remember { mutableStateOf(false) }
    var dialog by remember { mutableStateOf<Dialog?>(null) }
    LaunchedEffect(ready) {
        if (!ready && dialog != Dialog.Maintenance) dialog = null
    }
    Surface(Modifier.fillMaxSize(), color = Paper) {
        BoxWithConstraints(Modifier.safeDrawingPadding().fillMaxSize(), contentAlignment = Alignment.TopCenter) {
            val courtHeight = if (maxHeight < 650.dp) 117.dp else 155.dp
            Column(
                Modifier.widthIn(max = 510.dp).fillMaxWidth().verticalScroll(rememberScrollState())
                    .padding(horizontal = 18.dp, vertical = 12.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    Box(Modifier.size(43.dp).clip(RoundedCornerShape(14.dp)).background(Teal), contentAlignment = Alignment.Center) {
                        Mark(CourtMark.Ball, Lemon, Modifier.size(31.dp))
                    }
                    Spacer(Modifier.width(10.dp))
                    Text("PickleScore", Modifier.weight(1f), color = Ink, fontSize = 26.sp, fontWeight = FontWeight.Black)
                    Box {
                        IconButton(onClick = { menu = true }, modifier = Modifier.semantics { contentDescription = "Game menu" }) {
                            Mark(CourtMark.Menu, Ink)
                        }
                        DropdownMenu(expanded = menu, onDismissRequest = { menu = false }) {
                            DropdownMenuItem(text = { Text("End Game") }, enabled = playing, onClick = {
                                menu = false; dialog = Dialog.End
                            })
                            DropdownMenuItem(text = { Text("Reset") }, enabled = ready, onClick = {
                                menu = false; dialog = Dialog.Reset
                            })
                            if (status.phase.hasLinkActions) {
                                DropdownMenuItem(text = { Text("Disconnect") }, onClick = { menu = false; onDisconnect() })
                            }
                            DropdownMenuItem(text = { Text("Maintenance") }, onClick = { menu = false; dialog = Dialog.Maintenance })
                        }
                    }
                }
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                    Row(
                        Modifier.weight(1f, fill = false).background(if (ready) Color(0xFFE1F1D9) else Color(0xFFEDEADE), CircleShape)
                            .semantics { liveRegion = LiveRegionMode.Polite }.padding(horizontal = 12.dp, vertical = 7.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(7.dp),
                    ) {
                        Box(Modifier.size(8.dp).clip(CircleShape).background(if (ready) Color(0xFF327245) else Muted))
                        Text(status.phase.label, color = Ink, fontSize = 13.sp, fontWeight = FontWeight.Bold)
                    }
                    TextButton(onClick = { dialog = Dialog.Preview }, enabled = ready) {
                        Mark(CourtMark.Board, if (ready) Teal else Muted, Modifier.size(20.dp))
                        Spacer(Modifier.width(5.dp))
                        Text("LED preview", fontWeight = FontWeight.Bold)
                    }
                }
                if (!ready) {
                    status.detail?.let { Text(it, color = Muted, style = MaterialTheme.typography.bodyMedium) }
                    connectionActions()
                }
                CourtCard(live, courtHeight)
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("Rally winner", color = Ink, fontSize = 19.sp, fontWeight = FontWeight.ExtraBold)
                    Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                        CourtKey("Rally Left", onRallyLeft, Modifier.weight(1f), playing, CourtMark.Left,
                            face = Lemon, edge = Color(0xFFB1BC4B), tall = true)
                        CourtKey("Rally Right", onRallyRight, Modifier.weight(1f), playing, CourtMark.Right,
                            face = Aqua, edge = Color(0xFF56AA96), tall = true)
                    }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    CourtKey("Undo", onUndo, Modifier.weight(1f), ready && live?.canUndo != false, CourtMark.Undo)
                    CourtKey("Switch Courts", onSwitchCourts, Modifier.weight(1f), playing, CourtMark.Switch)
                }
                if (live?.historyBlocked == true) {
                    Text("Board memory is low. Game action was not recorded.",
                        Modifier.semantics { liveRegion = LiveRegionMode.Polite }, color = Ink)
                } else if (ready && status.pendingWrites > 0) {
                    Text(status.transportText, Modifier.semantics { liveRegion = LiveRegionMode.Polite }, color = Muted, fontSize = 13.sp)
                }
            }
        }
    }
    when (dialog) {
        Dialog.End -> if (playing) {
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
        Dialog.Reset -> if (live != null) {
            ResetDialog(live.gameMode, { dialog = null }) { side, mode ->
                if (dialog == Dialog.Reset) {
                    dialog = null
                    onReset(side, mode)
                }
            }
        }
        Dialog.Preview -> if (live != null) {
            AlertDialog(
                onDismissRequest = { dialog = null }, title = { Text("LED preview") },
                text = {
                    Column(Modifier.fillMaxWidth().verticalScroll(rememberScrollState()), horizontalAlignment = Alignment.CenterHorizontally) {
                        Box(Modifier.width(72.dp)) { ScoreboardPreview(live) }
                        if (live.canUndo == null) {
                            Spacer(Modifier.height(10.dp))
                            Text("Legacy board: the Singles layout may differ and Undo may be limited.")
                        }
                    }
                },
                confirmButton = { TextButton(onClick = { dialog = null }) { Text("Close") } },
            )
        }
        Dialog.Maintenance -> AlertDialog(
            onDismissRequest = { dialog = null }, title = { Text("Board maintenance") },
            text = { Text("Disconnect before using the authorized PickleScore maintenance tool.") },
            confirmButton = { TextButton(onClick = { dialog = null }) { Text("Close") } },
        )
        null -> Unit
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
            // Spec 03 Section 4: announce the same score call as the visible preview.
            val server = if (state.gameMode == GameMode.DOUBLES) "server ${state.serverNumber}, " else ""
            "Scoreboard: serving score ${state.servingScore}, opponent ${state.opponentScore}, " +
                "$server${state.servingSide.name.lowercase()} serving"
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
