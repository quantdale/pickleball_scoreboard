package com.example.pickleballscoreboard.ble

import android.Manifest
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
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import androidx.core.content.ContextCompat
import com.example.pickleballscoreboard.state.ScoreboardState
import java.util.UUID

// Spec 02 Sections 3..7. All session/queue/UI mutations run on the main thread.
@Suppress("DEPRECATION")
class BleClient(
    private val context: Context,
) {
    companion object {
        const val DEVICE_NAME = "PickleScore"
        val SERVICE_UUID: UUID = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b")
        val COMMAND_UUID: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8")
        val LEGACY_STATE_UUID: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a9")
        val MANUAL_STATE_UUID: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26ac")
        val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        const val PORTRAIT_FIRMWARE = "This board runs portrait firmware. Update the board before using this controller"
    }

    private val main = Handler(Looper.getMainLooper())
    private val adapter by lazy { (context.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)?.adapter }
    private var scanner: BluetoothLeScanner? = null
    private var scanCallback: ScanCallback? = null
    private var gatt: BluetoothGatt? = null
    private var commandChar: BluetoothGattCharacteristic? = null
    private var stateUuid: UUID? = null
    private val readiness = GattReadiness()
    private val writes = CommandWriteQueue()
    private var onStateCallback: ((ScoreboardState) -> Unit)? = null
    private var onErrorCallback: ((String) -> Unit)? = null
    private var onConnectedCallback: (() -> Unit)? = null
    private var onSynchronizingCallback: (() -> Unit)? = null
    private var onPendingWrites: ((Int) -> Unit)? = null
    private var syncDeadline: Runnable? = null
    private var writeDeadline: Runnable? = null

    fun hasPermissions(): Boolean {
        val permissions =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                listOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
            } else {
                listOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.BLUETOOTH)
            }
        return permissions.all { ContextCompat.checkSelfPermission(context, it) == PackageManager.PERMISSION_GRANTED }
    }

    fun startScan(
        onFound: (BluetoothDevice) -> Unit,
        onError: (String) -> Unit,
    ) {
        stopScan()
        if (!hasPermissions()) {
            onError("Missing BLE permissions")
            return
        }
        val callback =
            object : ScanCallback() {
                override fun onScanResult(
                    callbackType: Int,
                    result: ScanResult?,
                ) {
                    val device = result?.device ?: return
                    val name = result.scanRecord?.deviceName
                    if (name == DEVICE_NAME || name == null) {
                        main.post {
                            if (scanCallback === this) onFound(device)
                        }
                    }
                }

                override fun onScanFailed(errorCode: Int) {
                    main.post {
                        if (scanCallback === this) {
                            stopScan()
                            onError("Scan failed: $errorCode")
                        }
                    }
                }
            }
        try {
            // Spec 02 §§6–7: permission may be revoked after the initial check.
            if (adapter?.isEnabled != true) {
                onError("Bluetooth is unavailable or disabled")
                return
            }
            scanner = adapter?.bluetoothLeScanner
            if (scanner == null) {
                onError("BLE scanner unavailable")
                return
            }
            scanCallback = callback
            val filter = ScanFilter.Builder().setServiceUuid(ParcelUuid(SERVICE_UUID)).build()
            scanner?.startScan(listOf(filter), ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), callback)
        } catch (e: SecurityException) {
            stopScan()
            onError("BLE scan permission denied")
        }
    }

    fun stopScan() {
        try {
            scanCallback?.let { scanner?.stopScan(it) }
        } catch (_: SecurityException) {
            // permission revoked
        }
        scanCallback = null
        scanner = null
    }

    fun connect(
        device: BluetoothDevice,
        onState: (ScoreboardState) -> Unit,
        onError: (String) -> Unit,
        onConnected: () -> Unit,
        onSynchronizing: () -> Unit,
        onPendingWrites: (Int) -> Unit,
    ) {
        // Spec 02 Sections 6–7: a permission change must not leave the old scan/session active.
        stopScan()
        disconnect()
        if (!hasPermissions()) {
            onError("Missing BLE permissions")
            return
        }
        onStateCallback = onState
        onErrorCallback = onError
        onConnectedCallback = onConnected
        onSynchronizingCallback = onSynchronizing
        this.onPendingWrites = onPendingWrites
        try {
            val connection = device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
            gatt = connection
            if (connection == null) {
                disconnect()
                onError("Could not create BLE connection")
                return
            }
            val deadline =
                Runnable {
                    if (gatt === connection && !readiness.ready) fail(connection, "Timed out waiting for subscribed scoreboard state")
                }
            syncDeadline = deadline
            main.postDelayed(deadline, 15_000)
        } catch (_: SecurityException) {
            disconnect()
            onError("BLE connect permission denied")
        }
    }

    fun disconnect() {
        val old = gatt
        gatt = null // reject stale callbacks before asking the platform to close
        commandChar = null
        stateUuid = null
        readiness.clear()
        writes.clear()
        clearSyncDeadline()
        clearWriteDeadline()
        onStateCallback = null
        onErrorCallback = null
        onConnectedCallback = null
        onSynchronizingCallback = null
        onPendingWrites = null
        try {
            old?.disconnect()
        } catch (_: SecurityException) {
            // Still attempt to release the platform client.
        }
        try {
            old?.close()
        } catch (_: SecurityException) {
            // permission revoked
        }
    }

    private fun fail(
        connection: BluetoothGatt,
        message: String,
    ) {
        if (connection !== gatt) return
        val reportError = onErrorCallback
        disconnect()
        reportError?.invoke(message)
    }

    fun writeCommand(command: BoardCommand) {
        if (gatt == null || !readiness.ready) {
            onErrorCallback?.invoke("Scoreboard is not synchronized")
            return
        }
        if (!writes.enqueue(command.bytes)) {
            gatt?.let { fail(it, "Command queue full") }
            return
        }
        onPendingWrites?.invoke(writes.size)
        pumpWrite()
    }

    private fun clearWriteDeadline() {
        writeDeadline?.let(main::removeCallbacks)
        writeDeadline = null
    }

    private fun pumpWrite() {
        val connection = gatt ?: return
        val characteristic = commandChar ?: return
        val value = writes.next() ?: return
        characteristic.value = value
        characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        try {
            if (!connection.writeCharacteristic(characteristic)) {
                fail(connection, "Command write could not start")
                return
            }
            // A missing platform callback must not stall the FIFO or cause a retry.
            val deadline = Runnable { fail(connection, "Command response timed out") }
            writeDeadline = deadline
            main.postDelayed(deadline, 5_000)
        } catch (_: SecurityException) {
            fail(connection, "BLE write permission denied")
        }
    }

    private fun clearSyncDeadline() {
        syncDeadline?.let(main::removeCallbacks)
        syncDeadline = null
    }

    private fun announceReady() {
        if (!readiness.announceOnce()) return
        clearSyncDeadline()
        onConnectedCallback?.invoke()
    }

    private val callback =
        object : BluetoothGattCallback() {
            override fun onConnectionStateChange(
                connection: BluetoothGatt,
                status: Int,
                newState: Int,
            ) {
                main.post {
                    if (connection !== gatt) return@post
                    if (status != BluetoothGatt.GATT_SUCCESS) {
                        fail(connection, "Connection failed: $status")
                        return@post
                    }
                    when (newState) {
                        BluetoothProfile.STATE_CONNECTED -> {
                            onSynchronizingCallback?.invoke()
                            try {
                                if (!connection.discoverServices()) fail(connection, "Service discovery could not start")
                            } catch (_: SecurityException) {
                                fail(connection, "BLE discovery permission denied")
                            }
                        }

                        BluetoothProfile.STATE_DISCONNECTED -> {
                            fail(connection, "Disconnected")
                        }
                    }
                }
            }

            override fun onServicesDiscovered(
                connection: BluetoothGatt,
                status: Int,
            ) {
                main.post {
                    if (connection !== gatt) return@post
                    if (status != BluetoothGatt.GATT_SUCCESS) {
                        fail(connection, "Service discovery failed: $status")
                        return@post
                    }
                    val service = connection.getService(SERVICE_UUID)
                    val command = service?.getCharacteristic(COMMAND_UUID)
                    // Spec 02 §3: only Manual State; never send commands to portrait firmware.
                    val state = service?.getCharacteristic(MANUAL_STATE_UUID)
                    if (state == null && service?.getCharacteristic(LEGACY_STATE_UUID) != null) {
                        fail(connection, PORTRAIT_FIRMWARE)
                        return@post
                    }
                    val descriptor = state?.getDescriptor(CCCD_UUID)
                    if (command == null || state == null || descriptor == null ||
                        command.properties and BluetoothGattCharacteristic.PROPERTY_WRITE == 0 ||
                        state.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY == 0
                    ) {
                        fail(connection, "Scoreboard command/state/CCCD unavailable")
                        return@post
                    }
                    commandChar = command
                    stateUuid = state.uuid
                    try {
                        if (!connection.setCharacteristicNotification(state, true)) {
                            fail(connection, "Could not enable local notifications")
                            return@post
                        }
                        descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                        if (!connection.writeDescriptor(descriptor)) fail(connection, "CCCD write could not start")
                    } catch (_: SecurityException) {
                        fail(connection, "BLE subscription permission denied")
                    }
                }
            }

            override fun onDescriptorWrite(
                connection: BluetoothGatt,
                descriptor: BluetoothGattDescriptor,
                status: Int,
            ) {
                main.post {
                    if (connection !== gatt || descriptor.uuid != CCCD_UUID || descriptor.characteristic.uuid != stateUuid) return@post
                    if (status != BluetoothGatt.GATT_SUCCESS) {
                        fail(connection, "CCCD write failed: $status")
                        return@post
                    }
                    readiness.subscribed()
                    announceReady()
                }
            }

            override fun onCharacteristicChanged(
                connection: BluetoothGatt,
                characteristic: BluetoothGattCharacteristic,
                value: ByteArray,
            ) {
                val payload = value.toString(Charsets.UTF_8)
                main.post {
                    if (connection !== gatt || characteristic.uuid != stateUuid) return@post
                    val state = parseStatePayload(payload) ?: return@post
                    readiness.receivedState()
                    onStateCallback?.invoke(state)
                    announceReady()
                }
            }

            @Deprecated("Compatibility callback for Android 12 and earlier")
            override fun onCharacteristicChanged(
                connection: BluetoothGatt,
                characteristic: BluetoothGattCharacteristic,
            ) {
                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
                    onCharacteristicChanged(connection, characteristic, characteristic.value ?: return)
                }
            }

            override fun onCharacteristicWrite(
                connection: BluetoothGatt,
                characteristic: BluetoothGattCharacteristic,
                status: Int,
            ) {
                main.post {
                    if (connection !== gatt || characteristic.uuid != COMMAND_UUID) return@post
                    if (status != BluetoothGatt.GATT_SUCCESS) {
                        fail(connection, "Command write failed: $status")
                        return@post
                    }
                    if (!writes.complete()) return@post
                    clearWriteDeadline()
                    onPendingWrites?.invoke(writes.size)
                    pumpWrite()
                }
            }
        }
}
