# Court 1 bounded BLE diagnostic — 2026-11-19

**Repeat diagnostic passed: Court 1 was discovered, sent its initial score, and
resumed advertising after disconnect.** The first diagnostic below did not
find Court 1. Neither diagnostic activated maintenance or attempted OTA.

## First diagnostic: not discovered

| Check | Observation |
| --- | --- |
| Windows readiness | Intel Wireless Bluetooth and the Microsoft BLE Enumerator were present with no PnP problem. WinRT reported radio on, BLE central support and access allowed. No adapter settings changed. |
| One active scan | Started 06:13:14.249 UTC; normal watcher stop at 06:13:29.395 UTC (`SUCCESS`). Received 81 advertisement callbacks from six distinct addresses. No callback matched the pinned Court 1 BLE address `3c:8a:1f:a1:ba:2a` or PickleScore name/service. No watcher error was reported. |
| GATT and recovery | Zero connections attempted, so no CCCD subscription, state notification or post-disconnect advertising check. No writes, maintenance activation, USB/serial access, reset, wiring changes or repeat scan. |
| Firmware state | No live firmware identity or health check. `court1-return-002` / `ota_0` remains the last verified firmware, not a finding of this scan. |

The immediate OTA blocker was failure to discover the pinned address. This scan
shows that Windows was able to receive other advertisements; it does not show
whether Court 1 was transmitting or whether its signal was reachable by this
host. It does not establish a firmware fault or that a scoring controller was
connected. The [portrait OTA attempt](court1-portrait-ota-attempt.md) stopped
before activation or upload.

The session evidence is outside Git in `20261119-pi-court1-ble-diagnostic/`:
`adapter-winrt.json`, `windows-adapter.log`, `result.json`, `evidence-audit.log`,
and `before.jsonl` (timestamped callbacks, including unrelated devices).

## Separately requested repeat: passed

- Windows again reported BLE central support, radio on and access allowed. The
  15-second active scan received 155 callbacks from nine addresses, including
  70 from the pinned Court 1 address. The first positive address/service match
  was at **06:17:43.910 UTC**, RSSI −72 dBm. Both scan watchers stopped normally.
- One GATT connection with uncached services confirmed PickleScore and the
  State Notify characteristic. Initial Notify returned `0,0,L,2,0,D` without
  a scoring command. The client unsubscribed and disconnected normally at
  **06:18:01.579 UTC**. A fresh pinned address/service advertisement followed
  **4.07 seconds** later (−68 dBm), without a second connection.
- No application write, maintenance activation, firmware readback, OTA, reset,
  USB/serial access, or rewiring. The changed discovery outcome does not explain
  why the earlier scans missed the board. The previous OTA authorization remains
  spent; a live source check and separate authorization are still required.

Repeat evidence is outside Git in `20261119-pi-court1-ble-diagnostic-repeat/`:
`adapter-winrt.json`, `result.json`, `console.log`, `before.jsonl` and `after.jsonl`.
