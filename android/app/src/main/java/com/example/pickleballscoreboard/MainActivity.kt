package com.example.pickleballscoreboard

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.material3.Button
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
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
import com.example.pickleballscoreboard.ui.ConnectionPhase
import com.example.pickleballscoreboard.ui.ControllerStatus
import com.example.pickleballscoreboard.ui.CourtTheme
import com.example.pickleballscoreboard.ui.ScoreboardScreen

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent { PickleballScoreboardApp() }
    }
}

@Composable
private fun PickleballScoreboardApp() {
    val context = LocalContext.current
    val bleClient = remember(context) { BleClient(context) }
    DisposableEffect(bleClient) {
        onDispose {
            bleClient.stopScan()
            bleClient.disconnect()
        }
    }

    var status by remember { mutableStateOf(ControllerStatus()) }
    var state by remember { mutableStateOf<ScoreboardState?>(null) }
    val devices = remember { mutableStateListOf<BluetoothDevice>() }

    fun error(message: String) {
        state = null
        devices.clear()
        status = ControllerStatus(ConnectionPhase.ReconnectRequired, "$message. Scan again and check the board before scoring.")
    }

    fun scan() {
        devices.clear()
        status = ControllerStatus(ConnectionPhase.Scanning, "Select your PickleScore board when it appears.")
        bleClient.startScan(
            onFound = { if (it !in devices) devices.add(it) },
            onError = ::error,
        )
    }

    val permissionLauncher =
        rememberLauncherForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
            if (bleClient.hasPermissions()) scan() else error("Bluetooth permissions denied")
        }

    fun startScan() {
        if (bleClient.hasPermissions()) {
            scan()
        } else {
            val permissions =
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
                } else {
                    arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
                }
            permissionLauncher.launch(permissions)
        }
    }

    fun disconnect() {
        bleClient.stopScan()
        bleClient.disconnect()
        state = null
        devices.clear()
        status = ControllerStatus()
    }

    fun connect(device: BluetoothDevice) {
        if (status.phase != ConnectionPhase.Scanning) return
        state = null
        status = ControllerStatus(ConnectionPhase.Connecting, "Opening Bluetooth link")
        bleClient.connect(
            device = device,
            onState = { state = it },
            onError = ::error,
            onSynchronizing = { status = ControllerStatus(ConnectionPhase.Synchronizing, "Waiting for the board’s score") },
            onConnected = { status = ControllerStatus(ConnectionPhase.Ready) },
            onPendingWrites = { status = status.copy(pendingWrites = it, submitted = true) },
        )
        devices.clear()
    }

    fun send(
        command: Byte,
        vararg arguments: Byte,
    ) {
        if (status.ready) bleClient.writeCommand(byteArrayOf(command, *arguments))
    }

    CourtTheme {
        ScoreboardScreen(
            state = state,
            status = status,
            onDisconnect = ::disconnect,
            onRallyLeft = { send(BleClient.CMD_RALLY_WON_LEFT) },
            onRallyRight = { send(BleClient.CMD_RALLY_WON_RIGHT) },
            onUndo = { send(BleClient.CMD_UNDO) },
            onSwitchCourts = { send(BleClient.CMD_SWITCH_COURTS) },
            onEndGame = { send(BleClient.CMD_END_GAME) },
            onReset = { side, mode ->
                send(
                    BleClient.CMD_RESET,
                    if (side == Side.LEFT) BleClient.CMD_RESET_SIDE_LEFT else BleClient.CMD_RESET_SIDE_RIGHT,
                    if (mode == GameMode.DOUBLES) BleClient.CMD_MODE_DOUBLES else BleClient.CMD_MODE_SINGLES,
                )
            },
        ) {
            ConnectionActions(status.phase, devices, ::startScan, ::disconnect, ::connect)
        }
    }
}

@Composable
private fun ConnectionActions(
    phase: ConnectionPhase,
    devices: List<BluetoothDevice>,
    onScan: () -> Unit,
    onStop: () -> Unit,
    onSelect: (BluetoothDevice) -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        when (phase) {
            ConnectionPhase.Scanning -> {
                OutlinedButton(onClick = onStop, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Stop scan") }
                for (device in devices) {
                    Button(onClick = { onSelect(device) }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) {
                        Text("${boardName(device)} · ${device.address}")
                    }
                }
            }

            ConnectionPhase.Disconnected, ConnectionPhase.ReconnectRequired -> {
                Button(onClick = onScan, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Scan for PickleScore") }
            }

            else -> {
                Unit
            } // Board menu offers cancellation during connect/sync.
        }
    }
}

@SuppressLint("MissingPermission") // Scan results are permission-gated; revocation can race a callback.
private fun boardName(device: BluetoothDevice): String =
    try {
        device.name ?: BleClient.DEVICE_NAME
    } catch (_: SecurityException) {
        BleClient.DEVICE_NAME
    }
