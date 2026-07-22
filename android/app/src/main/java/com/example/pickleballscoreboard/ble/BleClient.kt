package com.example.pickleballscoreboard.ble

import java.util.UUID

// BLE client stub.
// See docs/specs/02-ble-protocol.md for the authoritative GATT structure and packet formats.
class BleClient {
    companion object {
        const val DEVICE_NAME = "PickleScore"

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
    }

    // Scan for the "PickleScore" peripheral.
    fun startScan(onFound: () -> Unit = {}) {
        // TODO: implement BLE scan (Spec 02 Section 6).
        onFound()
    }

    // Connect to the device at the given address.
    fun connect(deviceAddress: String) {
        // TODO: implement GATT connection.
    }

    // Subscribe to state notifications and forward the raw payload string.
    fun subscribeToState(onState: (String) -> Unit = {}) {
        // TODO: enable notifications on STATE_UUID and parse payload (Spec 02 Section 5).
        onState("")
    }

    // Write a command byte sequence to the command characteristic.
    fun writeCommand(bytes: ByteArray) {
        // TODO: implement write to COMMAND_UUID (Spec 02 Section 4).
    }
}
