## Context

`setBrightness8` in the pinned driver (3.0.15) rewrites only OE bits in the one
DMA buffer, with no allocation and no refresh-rate change. Modelling its
arithmetic for this setup (64-clock rows, blanking 2, transition bit 0) shows
values 1–12 all give the minimum two-clock window, and larger values give whole
multiples of it. The Court 1 supply rating is undocumented. Court 1's OTA
headroom is already marginal (5.9 KB over the 49,152 B floor on the source
image), so persistent heap matters.

## Goals / Non-Goals

**Goals:** brighter on demand with no effect on scoring, Undo or the landscape
layout; a board-authoritative level that survives disconnects and restarts.

**Non-Goals:** dimmer than 12/255 (that needs grey pixel values, which touches
the deferred colour work); automatic or ambient brightness; certifying any level.

## Decisions

- **Report on the Manual State stream, not a new characteristic.** A tagged
  `B,<level>,<max>` payload costs no GATT object, descriptor or handle. A new
  Notify characteristic plus CCCD would add an estimated ~1 KB of persistent
  heap (Arduino BLE objects, semaphores and the descriptor buffer) and would
  fill the default 15-handle service table exactly. Android 2.0.0's exact
  parser already ignores non-state payloads (Spec 02 §7). Rejected: extending
  Manual State to eight fields, which would break every 2.0.0 controller.
- **Absolute levels (`B<n>`), not relative steps.** Idempotent, safe to
  coalesce, and nothing to replay after a reconnect.
- **Same FIFO as scoring in firmware; one coalesced, lowest-priority slot in
  Android.** At most one brightness write waits, behind every scoring write.
- **A level ladder, not raw 0–255.** Each step is a visibly distinct multiple
  (×1, 2, 3, 4, 6, 8, 12, 16), capped at 128/255. A build constant
  (`PICKLE_BRIGHTNESS_MAX_LEVEL`, default 4) bounds what the board accepts.
- **NVS with a 3 s settle delay.** At most one small write per burst of
  adjustments, none during maintenance, restored before the boot frame.

## Risks / Trade-offs

- [Unmeasured current at higher levels] → conservative ×4 cap and the Spec 03
  §2a physical acceptance test before relying on levels above 1 or raising it.
- [Brighter panels may expose the deferred colour or ghosting faults] →
  acceptance compares each level with level 1; a failed level lowers the cap.
- [Source-image OTA heap is marginal] → this change adds no persistent heap,
  but installing it over the air from `court-landscape-001` will probably hit
  the same `flash-begin` heap floor; deployment needs its own approval and plan.

## Migration Plan

Install the APK (in-place upgrade over 2.0.0) and the OTA candidate separately,
in either order. Rollback: reinstall `court-landscape-001`, which ignores the
stored NVS byte. Until the board is updated, the controller shows "Needs board
update".
