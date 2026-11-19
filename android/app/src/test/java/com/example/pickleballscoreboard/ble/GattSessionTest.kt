package com.example.pickleballscoreboard.ble

import org.junit.Assert.*
import org.junit.Test

class GattSessionTest {
    @Test
    fun subscriptionBeforeSnapshot() {
        val state = GattReadiness()
        assertFalse(state.ready)
        state.subscribed()
        assertFalse(state.announceOnce())
        state.receivedState()
        assertTrue(state.ready)
        assertTrue(state.announceOnce())
        assertFalse(state.announceOnce())
    }

    @Test
    fun snapshotBeforeDescriptorCallback() {
        val state = GattReadiness()
        state.receivedState()
        assertFalse(state.announceOnce())
        state.subscribed()
        assertTrue(state.announceOnce())
        state.receivedState()
        assertFalse(state.announceOnce())
    }

    @Test
    fun disconnectClearsReadinessAndRequiresFreshSnapshot() {
        val state = GattReadiness()
        state.subscribed()
        state.receivedState()
        assertTrue(state.announceOnce())
        state.clear()
        assertFalse(state.ready)
        state.subscribed()
        assertFalse(state.announceOnce())
        state.receivedState()
        assertTrue(state.announceOnce())
    }

    @Test
    fun writesRemainOrderedAtomicAndCopiedUntilAcknowledged() {
        val queue = CommandWriteQueue()
        val reset = "0RS".toByteArray()
        assertTrue(queue.enqueue(reset))
        reset[0] = 'X'.code.toByte()
        assertTrue(queue.enqueue("R".toByteArray()))
        assertTrue(queue.enqueue("U".toByteArray()))
        assertArrayEquals("0RS".toByteArray(), queue.next())
        assertNull(queue.next())
        queue.complete()
        assertArrayEquals("R".toByteArray(), queue.next())
        assertNull(queue.next())
        queue.complete()
        assertArrayEquals("U".toByteArray(), queue.next())
        queue.complete()
        assertNull(queue.next())
    }

    @Test
    fun rapidIdenticalTapsAreDistinctAndBacklogIncludesInFlightWrite() {
        val queue = CommandWriteQueue()
        assertFalse(queue.complete())
        assertTrue(queue.enqueue("L".toByteArray()))
        assertArrayEquals("L".toByteArray(), queue.next())
        assertTrue(queue.enqueue("L".toByteArray()))
        assertTrue(queue.enqueue("R".toByteArray()))
        assertEquals(3, queue.size)
        assertNull(queue.next())
        assertTrue(queue.complete())
        assertEquals(2, queue.size)
        assertFalse(queue.complete())
        assertArrayEquals("L".toByteArray(), queue.next())
        assertTrue(queue.complete())
        assertArrayEquals("R".toByteArray(), queue.next())
        queue.clear()
        assertFalse(queue.complete())
        assertEquals(0, queue.size)
    }

    @Test
    fun capacityIncludesTheOutstandingGattWrite() {
        val queue = CommandWriteQueue(2)
        assertTrue(queue.enqueue("L".toByteArray()))
        assertArrayEquals("L".toByteArray(), queue.next())
        assertTrue(queue.enqueue("R".toByteArray()))
        assertFalse(queue.enqueue("U".toByteArray()))
        assertEquals(2, queue.size)
        assertTrue(queue.complete())
        assertTrue(queue.enqueue("U".toByteArray()))
        assertArrayEquals("R".toByteArray(), queue.next())
    }

    @Test
    fun failedOrClosedSessionClearsPendingWrites() {
        val queue = CommandWriteQueue(2)
        assertTrue(queue.enqueue("L".toByteArray()))
        assertTrue(queue.enqueue("R".toByteArray()))
        assertFalse(queue.enqueue("E".toByteArray()))
        assertArrayEquals("L".toByteArray(), queue.next())
        queue.clear()
        assertNull(queue.next())
        assertTrue(queue.enqueue("0LD".toByteArray()))
        assertArrayEquals("0LD".toByteArray(), queue.next())
    }
}
