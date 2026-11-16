package com.example.pickleballscoreboard.ble

// Spec 02 Section 6: Android callbacks may arrive in either order.
class GattReadiness {
    private var subscribed = false
    private var receivedState = false
    private var announced = false
    val ready: Boolean get() = subscribed && receivedState

    fun subscribed() {
        subscribed = true
    }

    fun receivedState() {
        receivedState = true
    }

    fun announceOnce(): Boolean {
        if (!ready || announced) return false
        announced = true
        return true
    }

    fun clear() {
        subscribed = false
        receivedState = false
        announced = false
    }
}

// One acknowledged write at a time. Reset remains one three-byte command.
class CommandWriteQueue(
    private val capacity: Int = 32,
) {
    private val pending = ArrayDeque<ByteArray>()
    private var busy = false

    fun enqueue(bytes: ByteArray): Boolean {
        if (pending.size >= capacity) return false
        pending.addLast(bytes.copyOf())
        return true
    }

    fun next(): ByteArray? {
        if (busy || pending.isEmpty()) return null
        busy = true
        return pending.removeFirst()
    }

    fun complete() {
        busy = false
    }

    fun clear() {
        pending.clear()
        busy = false
    }
}
