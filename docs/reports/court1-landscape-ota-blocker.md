# Court 1: `court-landscape-001` OTA blocked by source heap

**Result: not installed.** Court 1 still runs `court-undo-003` on `ota_0`, VALID,
confirmed and healthy. BLE initial sync passed after every attempt, and Windows
Wi-Fi and Ethernet were restored each time.

## Exact device rejection

After the owner's power cycle (authenticated uptime 658 s), the authorized
attempt authenticated the source and passed the CLI memory gate. The board then
rejected the upload:

```text
HTTP 422  deviceReason: "insufficient or fragmented free heap"
```

The firmware returns this text from two places. Upload admission returns it with
**503**; `FlashSink::begin` (`ota_maintenance.cpp`, stage `flash-begin`) returns
it with **422**. So the board passed upload admission (≥ 49,152 B free after the
package header). Free internal heap then fell below the unchanged 49,152 B floor
during signature verification and receipt of the first image bytes. The largest
block was 30,708 B against the 4,096 B floor, so the free total failed, not
fragmentation. That check runs **before `esp_ota_begin`**, so nothing was erased
or written. Attempt 3's 422 reason was not captured; with even less heap it most
likely failed at the same check, but an erase then cannot be ruled out. The
`court-controller-002` fallback in `ota_1` is therefore probably intact but has
not been read back.

## Why a restart did not help

| Source firmware | Pre-upload free internal heap | Margin over 49,152 B | Outcome |
| --- | ---: | ---: | --- |
| `court-controller-002` → `court-undo-003` | 60,880 B | 11,728 B | Installed |
| `court-undo-003`, 4.7 h uptime (attempt 3) | 53,860 B | 4,708 B | 422, reason not captured |
| `court-undo-003`, fresh boot (attempt 6) | 55,044 B | 5,892 B | 422, reason above |

`court-undo-003` holds a static 6,144 B Undo journal that `court-controller-002`
did not. 60,880 − 6,144 = 54,736 B, which matches the measured maintenance heap.
The most plausible cause is that the running source firmware leaves too little
headroom for the upload path's transient allocations. Those allocations are
signature verification and, likely, Wi-Fi/lwIP receive buffers for the streamed
body. Their exact split is not measured: there was no serial capture, by design.

`court-landscape-001` keeps the same 6,144 B journal, so OTA updates *from* it
would meet the same limit.

## Attempts in this session

| Output | Stop | Upload |
| --- | --- | --- |
| `court1-deployment` | `wifi_peer_identity`: stale ARP from another SoftAP | No |
| `court1-deployment-2` | `memory_admission` (CLI gate, 4 h uptime) | No |
| `court1-deployment-3` | HTTP 422, reason discarded by the old CLI | Rejected before write |
| `court1-deployment-4` | DHCP lost, static fallback needs elevation | No |
| `court1-deployment-5` | `wifi_peer_identity`: ARP cache empty after TCP probe | No |
| `court1-deployment-6` | HTTP 422, `insufficient or fragmented free heap` | Rejected before write |

Evidence: `_AgentArtifacts/pickleball_scoreboard/sessions/20261120-claude-landscape/`.
Package `afe09108…2d46c5`, image `922091fa…55d6ba`, both verified offline,
including the ABI and build markers the installed firmware checks.

## Tooling changes

- `failure.deviceReason` keeps the firmware's own rejection text. Only fixed
  firmware literals or `ESP_ERR_*` names are kept, and a test keeps the allowlist
  in sync with the firmware source.
- `--expect-source-max-uptime-s` refuses to upload unless authenticated uptime
  proves a reported restart.
- When the ARP cache is empty, the peer guard sends one ARP request on the
  pinned adapter. The reply must still equal the AP BSSID.

## Options (each needs owner approval)

1. **Pace the first image bytes (host only).** Send the header and first 1 KiB,
   pause briefly so the board passes `flash-begin` before the rest of the body
   is buffered, then stream normally. This tests the receive-buffer hypothesis
   without lowering any floor. One more attempt, and `flash-begin` rejects
   before any write if it fails.
2. **Fix the landscape firmware's headroom before shipping** (for example a
   smaller or maintenance-released Undo journal), so later updates from it keep
   about 11 KB of margin. This does not help the current source.
3. **USB install** of `court-landscape-001`. This needs fresh isolation and
   separate authorization.
