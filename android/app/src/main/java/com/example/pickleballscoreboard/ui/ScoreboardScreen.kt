package com.example.pickleballscoreboard.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.example.pickleballscoreboard.state.ScoreboardState
import com.example.pickleballscoreboard.state.Side

// Main screen exposing the six control inputs from Spec 01 Section 3 and the
// 64×16 logical preview canvas from Spec 03 Section 2.
// The displayed state comes from BLE Notify payloads only; this screen does not
// compute scoring logic locally (Spec 02 Section 2).
@Composable
fun ScoreboardScreen(
    state: ScoreboardState,
    statusText: String,
    onDisconnect: () -> Unit,
    onRallyLeft: () -> Unit,
    onRallyRight: () -> Unit,
    onUndo: () -> Unit,
    onSwitchCourts: () -> Unit,
    onEndGame: () -> Unit,
    onReset: () -> Unit,
) {
    Column(
        modifier =
            Modifier
                .fillMaxSize()
                .padding(16.dp),
    ) {
        Text(
            text = statusText,
            style = MaterialTheme.typography.bodyMedium,
            modifier = Modifier.padding(bottom = 8.dp),
        )

        ScoreboardPreview(
            state = state,
            modifier =
                Modifier
                    .weight(1f)
                    .padding(bottom = 16.dp),
        )

        ControlButtons(
            onRallyLeft = onRallyLeft,
            onRallyRight = onRallyRight,
            onUndo = onUndo,
            onSwitchCourts = onSwitchCourts,
            onEndGame = onEndGame,
            onReset = onReset,
        )

        Button(
            onClick = onDisconnect,
            modifier =
                Modifier
                    .fillMaxWidth()
                    .padding(top = 8.dp),
        ) {
            Text("Disconnect")
        }
    }
}

@Composable
fun ScoreboardPreview(
    state: ScoreboardState,
    modifier: Modifier = Modifier,
) {
    // Spec 03 Section 6: preview uses the shared physical canvas aspect ratio.
    val context = LocalContext.current
    val renderer = remember { DisplayRenderer(context) }
    // The weighted screen region must not force the canvas itself to be tall.
    Box(modifier = modifier.fillMaxWidth(), contentAlignment = Alignment.Center) {
        Canvas(
            modifier =
                Modifier
                    .aspectRatio(renderer.aspectRatio)
                    .fillMaxWidth()
                    .background(Color.Black)
                    .semantics {
                        contentDescription = "Scoreboard: ${state.leftScore} to ${state.rightScore}, " +
                            "${state.servingSide.name.lowercase()} serving, server ${state.serverNumber}"
                    },
        ) {
            with(renderer) {
                render(state)
            }
        }
    }
}

@Composable
fun ControlButtons(
    onRallyLeft: () -> Unit,
    onRallyRight: () -> Unit,
    onUndo: () -> Unit,
    onSwitchCourts: () -> Unit,
    onEndGame: () -> Unit,
    onReset: () -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Button(onClick = onRallyLeft, modifier = Modifier.weight(1f)) {
                Text("Rally Left")
            }
            Button(onClick = onRallyRight, modifier = Modifier.weight(1f)) {
                Text("Rally Right")
            }
        }
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Button(onClick = onUndo, modifier = Modifier.weight(1f)) {
                Text("Undo")
            }
            Button(onClick = onSwitchCourts, modifier = Modifier.weight(1f)) {
                Text("Switch Courts")
            }
        }
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Button(onClick = onEndGame, modifier = Modifier.weight(1f)) {
                Text("End Game")
            }
            Button(onClick = onReset, modifier = Modifier.weight(1f)) {
                Text("Reset")
            }
        }
    }
}

@Preview
@Composable
fun ScoreboardScreenPreview() {
    ScoreboardScreen(
        state = ScoreboardState(servingSide = Side.LEFT),
        statusText = "Connected (preview)",
        onDisconnect = {},
        onRallyLeft = {},
        onRallyRight = {},
        onUndo = {},
        onSwitchCourts = {},
        onEndGame = {},
        onReset = {},
    )
}
