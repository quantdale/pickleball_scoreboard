package com.example.pickleballscoreboard.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
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
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.example.pickleballscoreboard.state.ScoreboardState
import com.example.pickleballscoreboard.state.Side

// Main screen exposing the six control inputs from Spec 01 Section 3 and the
// 64×32 logical preview canvas from Spec 03 Section 2.
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
    onReset: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp)
    ) {
        Text(
            text = statusText,
            style = MaterialTheme.typography.bodyMedium,
            modifier = Modifier.padding(bottom = 8.dp)
        )

        ScoreboardPreview(
            state = state,
            modifier = Modifier
                .weight(1f)
                .padding(bottom = 16.dp)
        )

        ControlButtons(
            onRallyLeft = onRallyLeft,
            onRallyRight = onRallyRight,
            onUndo = onUndo,
            onSwitchCourts = onSwitchCourts,
            onEndGame = onEndGame,
            onReset = onReset
        )

        Button(
            onClick = onDisconnect,
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 8.dp)
        ) {
            Text("Disconnect")
        }
    }
}

@Composable
fun ScoreboardPreview(state: ScoreboardState, modifier: Modifier = Modifier) {
    // 64×32 logical canvas scaled to the widest available width while keeping
    // the 2:1 aspect ratio (Spec 03 Section 2).
    val context = LocalContext.current
    val renderer = remember { DisplayRenderer(context) }
    Canvas(
        modifier = modifier
            .fillMaxWidth()
            .aspectRatio(64f / 32f)
            .background(Color.Black)
    ) {
        with(renderer) {
            render(state)
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
    onReset: () -> Unit
) {
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
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
            horizontalArrangement = Arrangement.spacedBy(8.dp)
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
            horizontalArrangement = Arrangement.spacedBy(8.dp)
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
        onReset = {}
    )
}
