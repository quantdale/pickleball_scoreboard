package com.example.pickleballscoreboard.ble

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.ParcelUuid
import android.util.Log
import androidx.core.content.ContextCompat
import com.example.pickleballscoreboard.state.GameMode
import com.example.pickleballscoreboard.state.ScoreboardState
import com.example.pickleballscoreboard.state.Side
import java.util.UUID

// BLE central implementation.
// See docs/specs/02-ble-protocol.md for the authoritative GATT structure and packet formats.
class BleClient(private val context: Context) {

    companion object {
        const val DEVICE_NAME = "PickleScore"
        const val TAG = "BleClient"

        // Spec 02 Section 3 UUIDs.
        val SERVICE_UUID: UUID = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b")
        val COMMAND_UUID: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8")
        val STATE_UUID: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a9")

        // Spec 02 Section 4 command bytes.
        const val CMD_RALLY_WON_LEFT: Byte = 'L'.code.toByte()
        const val CMD_RALLY_WON_RIGHT: Byte = 'R'.code.toByte()
        const val CMD_UNDO: Byte = 'U'.code.toByte()
        const val CMD_SWITCH_COURTS: Byte = 'C'.code.toByte()
        const val CMD_END_GAME: Byte = 'E'.code.toByte()
        const val CMD_RESET: Byte = '0'.code.toByte()

        // Spec 02 Section 4a: reset command's side and mode bytes. Kept as
        // distinct constants from CMD_RALLY_WON_LEFT/RIGHT even though the
        // byte values coincide today — Reset and Rally-Won are logically
        // unrelated commands and shouldn't be coupled through shared constants.
        const val CMD_RESET_SIDE_LEFT: Byte = 'L'.code.toByte()
        const val CMD_RESET_SIDE_RIGHT: Byte = 'R'.code.toByte()
        const val CMD_MODE_DOUBLES: Byte = 'D'.code.toByte()
        const val CMD_MODE_SINGLES: Byte = 'S'.code.toByte()
    }

    private val bluetoothAdapter: BluetoothAdapter? by lazy {
        val manager = context.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager
        manager?.adapter
    }

    private var scanner: BluetoothLeScanner? = null
    private var scanCallback: ScanCallback? = null
    private var gatt: BluetoothGatt? = null

    private var commandChar: BluetoothGattCharacteristic? = null
    private var stateChar: BluetoothGattCharacteristic? = null

    private var onStateCallback: ((ScoreboardState) -> Unit)? = null
    private var onErrorCallback: ((String) -> Unit)? = null
    private var onConnectedCallback: (() -> Unit)? = null

    // Runtime permission check for BLE scan/connect.
    fun hasPermissions(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED
                    && ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED
        } else {
            ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
                    && ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH) == PackageManager.PERMISSION_GRANTED
        }
    }

    // Scan for the "PickleScore" peripheral advertising the Spec 02 service UUID.
    fun startScan(onFound: (BluetoothDevice) -> Unit, onError: (String) -> Unit) {
        if (!hasPermissions()) {
            onError("Missing BLE permissions")
            return
        }

        val adapter = bluetoothAdapter
        if (adapter == null || !adapter.isEnabled) {
            onError("Bluetooth is not available or disabled")
            return
        }

        stopScan()

        val filter = ScanFilter.Builder()
            .setServiceUuid(ParcelUuid(SERVICE_UUID))
            .build()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        val callback = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult?) {
                val device = result?.device ?: return
                val name = result.scanRecord?.deviceName
                if (name == DEVICE_NAME || name == null) {
                    onFound(device)
                }
            }

            override fun onScanFailed(errorCode: Int) {
                onError("Scan failed with error code $errorCode")
            }
        }

        scanCallback = callback
        scanner = adapter.bluetoothLeScanner
        scanner?.startScan(listOf(filter), settings, callback)
    }

    fun stopScan() {
        scanCallback?.let {
            try {
                scanner?.stopScan(it)
            } catch (e: SecurityException) {
                Log.w(TAG, "stopScan security exception", e)
            }
        }
        scanCallback = null
        scanner = null
    }

    // Connect to the device and subscribe to State notifications.
    fun connect(
        device: BluetoothDevice,
        onState: (ScoreboardState) -> Unit,
        onError: (String) -> Unit,
        onConnected: () -> Unit = {}
    ) {
        if (!hasPermissions()) {
            onError("Missing BLE permissions")
            return
        }

        stopScan()

        onStateCallback = onState
        onErrorCallback = onError
        onConnectedCallback = onConnected

        try {
            gatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
        } catch (e: SecurityException) {
            onError("Missing BLE connect permission: ${e.message}")
        }
    }

    fun disconnect() {
        try {
            gatt?.disconnect()
            gatt?.close()
        } catch (e: SecurityException) {
            Log.w(TAG, "disconnect security exception", e)
        }
        gatt = null
        commandChar = null
        stateChar = null
    }

    // Write a command byte sequence to the Command characteristic.
    fun writeCommand(bytes: ByteArray) {
        val char = commandChar
        val gatt = gatt
        if (char == null || gatt == null) {
            onErrorCallback?.invoke("Not connected")
            return
        }

        char.value = bytes
        char.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT

        try {
            val success = gatt.writeCharacteristic(char)
            if (!success) {
                onErrorCallback?.invoke("writeCharacteristic returned false")
            }
        } catch (e: SecurityException) {
            onErrorCallback?.invoke("Missing BLE write permission: ${e.message}")
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                onErrorCallback?.invoke("Connection state change error: $status")
                return
            }

            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    try {
                        gatt.discoverServices()
                    } catch (e: SecurityException) {
                        onErrorCallback?.invoke("Missing BLE service discovery permission: ${e.message}")
                    }
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    onErrorCallback?.invoke("Disconnected")
                }
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                onErrorCallback?.invoke("Service discovery failed: $status")
                return
            }

            val service = gatt.getService(SERVICE_UUID)
            if (service == null) {
                onErrorCallback?.invoke("PickleScore service not found")
                return
            }

            commandChar = service.getCharacteristic(COMMAND_UUID)
            stateChar = service.getCharacteristic(STATE_UUID)

            if (commandChar == null || stateChar == null) {
                onErrorCallback?.invoke("Command or State characteristic not found")
                return
            }

            // Enable notifications on the State characteristic.
            try {
                val success = gatt.setCharacteristicNotification(stateChar, true)
                if (!success) {
                    onErrorCallback?.invoke("Failed to enable State notifications")
                    return
                }
            } catch (e: SecurityException) {
                onErrorCallback?.invoke("Missing BLE notification permission: ${e.message}")
                return
            }

            val descriptor = stateChar?.getDescriptor(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"))
            if (descriptor != null) {
                descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                try {
                    gatt.writeDescriptor(descriptor)
                } catch (e: SecurityException) {
                    onErrorCallback?.invoke("Missing BLE descriptor write permission: ${e.message}")
                }
            }

            onConnectedCallback?.invoke()
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            if (characteristic.uuid != STATE_UUID) return

            val payload = value.toString(Charsets.UTF_8)
            val newState = parseStatePayload(payload)
            if (newState != null) {
                onStateCallback?.invoke(newState)
            } else {
                Log.w(TAG, "Ignoring malformed state payload: $payload")
            }
        }

        @Deprecated("Deprecated in Android 13 and below, kept for compatibility.")
        @Suppress("DEPRECATION")
        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
                val value = characteristic.value ?: return
                onCharacteristicChanged(gatt, characteristic, value)
            }
        }

        override fun onCharacteristicWrite(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int
        ) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                onErrorCallback?.invoke("Characteristic write failed: $status")
            }
        }
    }
}

// Parse a Spec 02 Section 5 state payload into ScoreboardState.
// Returns null for any malformed payload, which the caller must ignore.
fun parseStatePayload(payload: String): ScoreboardState? {
    val parts = payload.split(',')
    if (parts.size != 6) return null

    val leftScore = parts[0].toIntOrNull() ?: return null
    val rightScore = parts[1].toIntOrNull() ?: return null
    val servingSide = when (parts[2]) {
        "L" -> Side.LEFT
        "R" -> Side.RIGHT
        else -> return null
    }
    val serverNumber = parts[3].toIntOrNull() ?: return null
    if (serverNumber != 1 && serverNumber != 2) return null
    val gameEnded = when (parts[4]) {
        "0" -> false
        "1" -> true
        else -> return null
    }
    val gameMode = when (parts[5]) {
        "D" -> GameMode.DOUBLES
        "S" -> GameMode.SINGLES
        else -> return null
    }

    return ScoreboardState(
        leftScore = leftScore,
        rightScore = rightScore,
        servingSide = servingSide,
        serverNumber = serverNumber,
        gameEnded = gameEnded,
        gameMode = gameMode
    )
}
