# Portrait controller design

## Decisions

- Keep the `portrait-r180-002` mount reference: horizontal `(63-y, x)` before the existing half swap, putting the original downstream panel on top. An upright block swap and a whole rotation have different signatures; require asymmetric physical evidence before changing the map. No wiring or Android rotation.
- Preserve full-height digits and move the opponent score down one row, leaving a two-row score gap and dark perimeter and seam. Share the position with Android. Stream lit pixels without intermediate heap or a second frame.
- Normal installed OTA uses existing BLE/SoftAP with explicit operational and write attestations. Existing USB-only isolation applies only to physical service. Export authenticated heap/stack evidence and drop stale WLAN association before bounded post-install observation; do not change OTA trust or memory floors.
- Keep the queue and readiness gates. Show queue backlog separately from the latest authoritative notification; this protocol cannot correlate individual commands with a notification and no-ops may not notify.
- Use discrete clicks with no timing-based rally debounce, confirmations for destructive actions, and no automatic retries. On transport uncertainty reconnect and inspect the board.

## Interface references

- [YouTube TV remote](https://refero.design/screens/6ed651e2-5cb8-482e-b0cb-cdaebbf06550): oversized input targets and separate connection header; use explicit rally labels rather than icon-only controls.
- [Apple Sports](https://refero.design/screens/76d6a8a2-4355-4739-82e7-6da5ddc49c26): visually isolate the score and phase; skip tabs and play-by-play.
- [Opal](https://refero.design/screens/43b18944-b33c-401d-88f8-650bf13f82e9) and [Miles](https://refero.design/screens/3eae009f-59ff-44d0-8442-360e0b20c166): name the connection phase and separate destructive actions. No animation or borrowed palette.

## Migration

Pair the app with portrait firmware after separately authorized commissioning. See the operator guide for mounting and rollback.
