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

    // One unsent write per setting, keyed by its command letter ('B' brightness, 'K' color).
    private val settings = LinkedHashMap<Byte, ByteArray>()
    private var settingInFlight: ByteArray? = null
    val size: Int get() = pending.size + settings.size + if (busy) 1 else 0

    // A write for this setting is queued or in flight; its report has not been superseded.
    fun settingPending(kind: Char): Boolean = settings.containsKey(kind.code.toByte()) || settingInFlight(kind)

    fun settingInFlight(kind: Char): Boolean = settingInFlight?.first() == kind.code.toByte()

    fun cancelUnsentSetting(kind: Char): Boolean = settings.remove(kind.code.toByte()) != null

    fun enqueue(bytes: ByteArray): Boolean {
        // Spec 02 §§6–7: include the outstanding write in the backlog bound.
        if (size >= capacity) return false
        pending.addLast(bytes.copyOf())
        return true
    }

    // Spec 02 §6: one coalesced slot per setting, yielding to every scoring write.
    // A newer value replaces an unsent one; the value already in flight needs no repeat.
    fun offerSetting(bytes: ByteArray) {
        val kind = bytes.first()
        if (settingInFlight?.contentEquals(bytes) == true) settings.remove(kind) else settings[kind] = bytes.copyOf()
    }

    fun next(): ByteArray? {
        if (busy) return null
        val value =
            pending.removeFirstOrNull() ?: settings.keys.firstOrNull()?.let { kind ->
                settings.remove(kind)?.also { settingInFlight = it }
            } ?: return null
        busy = true
        return value
    }

    fun complete(): Boolean {
        if (!busy) return false
        busy = false
        settingInFlight = null
        return true
    }

    fun clear() {
        pending.clear()
        busy = false
        settings.clear()
        settingInFlight = null
    }
}
