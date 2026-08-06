package com.example.pickleballscoreboard

import android.Manifest
import android.bluetooth.BluetoothDevice
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.example.pickleballscoreboard.ble.BleClient
import com.example.pickleballscoreboard.state.GameMode
import com.example.pickleballscoreboard.state.ScoreboardState
import com.example.pickleballscoreboard.state.Side
import com.example.pickleballscoreboard.ui.ScoreboardScreen

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            PickleballScoreboardApp()
        }
    }
}

@Composable
fun PickleballScoreboardApp() {
    val context = LocalContext.current
    val bleClient = remember { BleClient(context) }

    var statusText by remember { mutableStateOf("Not connected") }
    var isConnected by remember { mutableStateOf(false) }
    var isScanning by remember { mutableStateOf(false) }
    val foundDevices = remember { mutableStateListOf<BluetoothDevice>() }
    var scoreboardState by remember { mutableStateOf(ScoreboardState()) }
    var showResetDialog by remember { mutableStateOf(false) }

    val permissionLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { results ->
        if (results.all { it.value }) {
            statusText = "Permissions granted; ready to scan"
        } else {
            statusText = "BLE permissions denied"
        }
    }

    fun requestPermissions() {
        val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT
            )
        } else {
            arrayOf(
                Manifest.permission.ACCESS_FINE_LOCATION,
                Manifest.permission.BLUETOOTH,
                Manifest.permission.BLUETOOTH_ADMIN
            )
        }
        permissionLauncher.launch(permissions)
    }

    fun startScan() {
        if (!bleClient.hasPermissions()) {
            requestPermissions()
            return
        }
        foundDevices.clear()
        isScanning = true
        statusText = "Scanning..."
        bleClient.startScan(
            onFound = { device ->
                if (!foundDevices.contains(device)) {
                    foundDevices.add(device)
                }
            },
            onError = { error ->
                isScanning = false
                statusText = error
            }
        )
    }

    fun stopScan() {
        bleClient.stopScan()
        isScanning = false
        if (statusText == "Scanning...") {
            statusText = "Scan stopped"
        }
    }

    fun connect(device: BluetoothDevice) {
        stopScan()
        statusText = "Connecting..."
        bleClient.connect(
            device = device,
            onState = { newState ->
                scoreboardState = newState
            },
            onError = { error ->
                isConnected = false
                statusText = error
            },
            onConnected = {
                isConnected = true
                statusText = "Connected to ${device.name ?: device.address}"
            }
        )
    }

    fun disconnect() {
        bleClient.disconnect()
        isConnected = false
        statusText = "Disconnected"
    }

    fun sendCommand(byte: Byte) {
        bleClient.writeCommand(byteArrayOf(byte))
    }

    fun sendReset(side: Side, mode: GameMode) {
        val sideByte = if (side == Side.LEFT) BleClient.CMD_RESET_SIDE_LEFT else BleClient.CMD_RESET_SIDE_RIGHT
        val modeByte = if (mode == GameMode.DOUBLES) BleClient.CMD_MODE_DOUBLES else BleClient.CMD_MODE_SINGLES
        bleClient.writeCommand(byteArrayOf(BleClient.CMD_RESET, sideByte, modeByte))
    }

    MaterialTheme {
        if (isConnected) {
            ScoreboardScreen(
                state = scoreboardState,
                statusText = statusText,
                onDisconnect = ::disconnect,
                onRallyLeft = { sendCommand(BleClient.CMD_RALLY_WON_LEFT) },
                onRallyRight = { sendCommand(BleClient.CMD_RALLY_WON_RIGHT) },
                onUndo = { sendCommand(BleClient.CMD_UNDO) },
                onSwitchCourts = { sendCommand(BleClient.CMD_SWITCH_COURTS) },
                onEndGame = { sendCommand(BleClient.CMD_END_GAME) },
                onReset = { showResetDialog = true }
            )

            if (showResetDialog) {
                ResetSideDialog(
                    onConfirm = { side, mode ->
                        sendReset(side, mode)
                        showResetDialog = false
                    },
                    onDismiss = { showResetDialog = false }
                )
            }
        } else {
            ConnectionScreen(
                statusText = statusText,
                isScanning = isScanning,
                foundDevices = foundDevices,
                onScanToggle = {
                    if (isScanning) stopScan() else startScan()
                },
                onDeviceSelected = ::connect
            )
        }
    }
}

@Composable
fun ConnectionScreen(
    statusText: String,
    isScanning: Boolean,
    foundDevices: List<BluetoothDevice>,
    onScanToggle: () -> Unit,
    onDeviceSelected: (BluetoothDevice) -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Text(text = statusText, style = MaterialTheme.typography.bodyLarge)

        Button(
            onClick = onScanToggle,
            modifier = Modifier.fillMaxWidth()
        ) {
            Text(if (isScanning) "Stop Scan" else "Scan for PickleScore")
        }

        LazyColumn(
            modifier = Modifier.weight(1f),
            verticalArrangement = Arrangement.spacedBy(4.dp)
        ) {
            items(foundDevices, key = { it.address }) { device ->
                val name = remember(device) { device.name ?: "Unknown device" }
                Button(
                    onClick = { onDeviceSelected(device) },
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text("$name (${device.address})")
                }
            }
        }
    }
}

@Composable
fun ResetSideDialog(
    onConfirm: (Side, GameMode) -> Unit,
    onDismiss: () -> Unit
) {
    // Spec 01 Section 9b: game mode is selected at the same time as the
    // 0-0-2 side, as part of Reset.
    var selectedMode by remember { mutableStateOf(GameMode.DOUBLES) }

    androidx.compose.material3.AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Reset game") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Singles or doubles?")
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(
                        onClick = { selectedMode = GameMode.DOUBLES },
                        enabled = selectedMode != GameMode.DOUBLES
                    ) {
                        Text("Doubles")
                    }
                    Button(
                        onClick = { selectedMode = GameMode.SINGLES },
                        enabled = selectedMode != GameMode.SINGLES
                    ) {
                        Text("Singles")
                    }
                }
                Text("Which side is 0-0-2?")
            }
        },
        confirmButton = {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = { onConfirm(Side.LEFT, selectedMode) }) {
                    Text("Left")
                }
                Button(onClick = { onConfirm(Side.RIGHT, selectedMode) }) {
                    Text("Right")
                }
            }
        },
        dismissButton = {
            Button(onClick = onDismiss) {
                Text("Cancel")
            }
        }
    )
}
