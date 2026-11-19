# Court 1 portrait firmware — authenticated no-write verification

**PASS, 2026-11-19.** In one separately authorized verification-only session,
Court 1 authenticated as `3c:8a:1f:a1:ba:28` and reported exact running build
`portrait-cw-001` on **`ota_1`**, SHA-256
`b039c3ca6e6e0d4a41077facb21802698c91de5f0e6b477d8ddf164ccadee5dd`,
**VALID**, `confirmed=true`, `healthy=true`. This supplies the independent
running-image evidence missing from the [prior install](court1-portrait-wireless-install.md).

| Checkpoint | Observation |
| --- | --- |
| BLE and AP | One authenticated BLE maintenance proof accepted. Fresh BSSID `3c:8a:1f:a1:ba:29` observed 12.935 s after BLE disconnect (−47 dBm, sample ~0.423 s old); one WPA2/CCMP association with pinned BSSID, DHCP and on-link routing. |
| Authenticated HTTP | `/api/session` then `/api/info` and `/api/status`. `agentProtocol=1`, uptime 1,333,441 ms, slot capacity 2,031,616 B. Current heap: 62,768 B at info and 62,432 B at idle status; zero bytes received/total and no staged build. Contiguous heap, minimum heap and stack watermarks were not exposed in this session. |
| Close and BLE recovery | `/api/close` acknowledged; AP link dropped 7.428 s later, before Windows restoration. A fresh pinned BLE advertisement followed; one scoring-service connection received initial Notify `0,0,L,2,0,D` without a scoring write. Normal disconnect; fresh advertising observed 3.243 s later. This is operational recovery, not independent RF proof of AP shutdown. |
| Host | Original disconnected Windows Wi-Fi state restored, temporary profile removed, radio states restored. Source-bound Ethernet TLS passed before, during and after. |

The only device HTTP calls were session login, info, status and close. No
firmware upload, staging, install, flash/metadata write, reboot, USB/serial
access, rollback, scoring command, or wiring change occurred. The inactive slot
was not raw-read. The `portrait-r180-002` package and archived horizontal
recovery package were independently reverified offline along with the running
image's corresponding signed package. Evidence is stored outside Git in
`20261119-pi-court1-portrait-verify/` (`result.json`, `console.log`,
`bss-samples.jsonl`, `association-samples.jsonl`, `offline.json`).

## Prepared next authorization — no write permission granted

The next proposed transition is `portrait-cw-001 / ota_1` →
`portrait-r180-002 / ota_0` on this exact device. Before any staging, require fresh
owner confirmation of idle court, controller disconnected, isolated USB-only
setup (panels off and discharged, HUB75 disconnected while unpowered, no external
positive 5 V), recovery access, and explicit approval for one signed attempt,
**overwrite of `court1-return-002` in `ota_0`**, normal OTA metadata writes,
downtime and score/Undo loss. Reserve at least five idle minutes, not including
physical acceptance. Preserve `portrait-cw-001` in `ota_1` and the separately
archived horizontal recovery package. Pay attention to the prior Windows link
behavior: verify the actual running image over authenticated HTTP, not from a
Wi-Fi disconnect or install acknowledgment alone. See the [rotation candidate
report](portrait-r180-verification.md#deployment-gates--not-authorized-here) for
exact digests and erase range. No retry, USB fallback or automatic recovery.
