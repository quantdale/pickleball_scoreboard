# Court 1 bounded BLE diagnostic — 2026-11-19

**Court 1 was not discovered. Initial synchronization and post-disconnect
advertising were not tested.** This was the one owner-authorized no-write
follow-up to the [portrait OTA attempt](court1-portrait-ota-attempt.md), which
also stopped at BLE discovery before activation or upload.

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
connected. Stop here. The smallest next check is an owner-operated scan in the
PickleScore app near the powered board, **without selecting a board**, to see
whether an independent nearby receiver lists Court 1. Do not reset or deploy.

The session evidence is outside Git in `20261119-pi-court1-ble-diagnostic/`:
`adapter-winrt.json`, `windows-adapter.log`, `result.json`, `evidence-audit.log`,
and `before.jsonl` (timestamped callbacks, including unrelated devices).
