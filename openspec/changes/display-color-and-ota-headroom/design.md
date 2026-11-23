## Context

The HUB75 driver expands RGB565 to 8-bit channels and applies a CIE table that
maps 255 to 255 and 0 to 0, so a fully-on channel lights every bit plane exactly
as White does today. Court 1's `court-landscape-001` reached a 59,456 B
network-ready heap; the measured upload consumes about 7.5 KB before the
49,152 B `flash-begin` floor and dips up to 24.6 KB below the session heap.

## Decisions

- **Palette of whole channels.** White, Red, Yellow, Green, Cyan, Blue. Mixed
  colors need partial channels whose low bit planes the minimum OE window cannot
  render evenly; magenta is a poor score color. Wire code is the color's letter.
- **Separate `K,<code>` report** after `B,<level>,<max>`, not an extra field:
  each payload stays exact and short, and controllers ignore what they do not
  parse.
- **One settings object and one 3 s settle** for brightness and color; only the
  changed NVS key is written.
- **Per-setting coalescing slot** in the Android write queue so a color tap
  never replaces a pending brightness write.
- **32-position Undo ring, not RTC memory.** The controller no longer offers
  Undo; Resume needs one position. Shrinking frees the bytes on every stage
  without relying on RTC slow-memory access behaviour that is unverified here.
- **Fix the timer leak in the existing `esp_timer_delete` wrapper.** IDF 4.4
  refuses to delete an armed timer and `ets_timer_done` drops the handle anyway;
  stop then delete, as IDF documents for owners releasing a timer.
- **Host headroom gate, never a lower floor.** Session heap must cover the
  measured demand above both device floors plus 3 KiB (60,456 B).

## Risks / Trade-offs

- [Timer fix is inferred from the measured caller and counts, not bench-proven]
  → status now reports armed deletes and recoveries; acceptance is a settled
  heap that no longer drifts across repeated closes.
- [Older 2.0.0 controllers lose deep Undo] → 32 recent positions remain.
- [Colors at the same level look dimmer than White] → documented; the cap is
  set by White, the worst current case.

## Migration Plan

Install 2.2.0 over 2.0.0 or 2.1.0. Install `court-color-001` once over USB
(the running `court-landscape-001` cannot pass its own upload heap check);
later releases update wirelessly. Rollback: reinstall the previous image; the
extra NVS byte is ignored.
