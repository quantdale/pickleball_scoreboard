# Portrait controller design

## Decisions

- Invert clockwise mounting before the existing half swap: horizontal `(y, 15-x)`; raw `(physicalDisplayX(y), 15-x)`. Counterclockwise would put the right panel on top and is intentionally not supported.
- Use fixed portrait bands, unchanged glyphs and white-on-black rendering. Stream lit pixels without intermediate heap or a second frame.
- Keep the queue and readiness gates. Show queue backlog separately from the latest authoritative notification; this protocol cannot correlate individual commands with a notification and no-ops may not notify.
- Use discrete clicks with no timing-based rally debounce, confirmations for destructive actions, and no automatic retries. On transport uncertainty reconnect and inspect the board.

## Interface references

- [YouTube TV remote](https://refero.design/screens/6ed651e2-5cb8-482e-b0cb-cdaebbf06550): oversized input targets and separate connection header; use explicit rally labels rather than icon-only controls.
- [Apple Sports](https://refero.design/screens/76d6a8a2-4355-4739-82e7-6da5ddc49c26): visually isolate the score and phase; skip tabs and play-by-play.
- [Opal](https://refero.design/screens/43b18944-b33c-401d-88f8-650bf13f82e9) and [Miles](https://refero.design/screens/3eae009f-59ff-44d0-8442-360e0b20c166): name the connection phase and separate destructive actions. No animation or borrowed palette.

## Migration

Pair the app with portrait firmware after separately authorized commissioning. See the operator guide for mounting and rollback.
