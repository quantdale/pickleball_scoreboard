# OTA reliability follow-up: evidence and first candidate

**Software only so far.** No serial, BLE, Wi-Fi or flash access to Court 1 in
this step. Court 1 still runs `court-color-002` on `ota_0`; `ota_1` holds
portrait `court-controller-002`. Change: `openspec/changes/ota-reliability-followup/`.
Evidence: `_AgentArtifacts/pickleball_scoreboard/sessions/20261121-claude-ota-followup/`,
which reanalyses the 2.2 logs in `20261121-claude-brightness/v22/`.

## What the 2.2 logs already show

### The deepest dip is at station join, not login or upload (task 1.1)

`min` is a global watermark, so a session's dip is only visible when it
undercuts the previous minimum.

| Session (build) | Network-ready free / min | Min before `session-admitted` | Dip |
|---|---:|---:|---:|
| `ota-deploy-4` (`court-color-001`, BLE) | 63,632 / 63,512 | 38,064 | **25,568 B** |
| `ota-deploy-1` (`court-color-001`, BLE) | 63,700 / 45,168 | 39,124 | ≥ 24,576 B |
| `verify-002b` (`court-color-002`, BLE) | 63,668 / 39,632 | 39,300 | ≥ 24,368 B |
| `ota-deploy-2` boot window, **no login** | 65,436 / 65,316 | 45,916 (at stop) | 19,520 B |

The last row has association and TCP connects but no HTTP reply and no login.
So the dip comes with the station joining and the client's first traffic. The
2.2 firmware logged nothing between `network-ready` and `session-admitted`, so
the step that allocates is not yet known.

### The CLI gate's body figure has no reproducible source (task 1.3)

`tools/ota_deploy.py` refuses an upload unless the session heap is at least
**60,456 B** = 32,768 + 24,616 + 3,072. Its comment cites
`docs/reports/ota-2.2-reliability.md`, which does not exist. The two recorded
wireless updates give:

| Update | Pre-upload status heap | Lowest heap during upload | Demand |
|---|---:|---:|---:|
| First (`agent-wpa2` → `agent-life2-budget`) | 59,492 | 38,692 | 20,800 B |
| Second (`agent-life2-budget` → `court1-return-002`) | 63,184 | 42,280 (sampled body low 42,400) | 20,904 B |

Neither pair reproduces 24,616 B. **The gate is unchanged.** It will be
recomputed only from upload measurements on the new image (task 1.3). The join
dip above is deeper than either upload's demand. Whether a burst like it can
recur during an upload is not known.

### Why the close reply can be lost (task 2.1, hypothesis)

- The reply leaves as several small writes: the status block, five custom
  headers, the blank line and the body. Nagle holds all but the first until
  Windows' delayed ACK.
- The pinned lwIP has a 3 s initial retransmission timeout
  (`CONFIG_LWIP_TCP_RTO_TIME 3000`).
- The 2.2 firmware stopped HTTP, then Wi-Fi, 500 ms after the acknowledgement.
  A single lost segment could therefore never be retransmitted.
- The client saw a `TimeoutError` at `response-headers` four times, and
  `ConnectionResetError` 10054 once.

A packet capture has not confirmed this yet.

### AP discovery measured the CLI's scan schedule (task 3.3)

| Sessions | BSS first seen | CLI scans |
|---|---|---|
| 5 BLE-activated sessions | 8.7 s or 17.6–17.9 s, about 0.5 s after a scan | 0, 8, 17 and 24 s |
| `cycle-3` | never within the 30 s deadline | 0, 8, 17 and 24 s |

The AP may have been up much earlier; the CLI only looked every 8–9 s.

### Cached lease (task 3.1)

| Session | Windows address | Result |
|---|---|---|
| `ota-deploy-3` | 192.168.4.2 `Preferred` immediately at association | TCP to port 80 failed for 41 s |
| `ota-deploy-2` | 192.168.4.2 | TCP connected, but `/api/info` got no reply |

Both happened while BLE advertising was still on (boot window, or before
login). The 2.2 firmware did not log DHCP, so whether the ESP32 ever assigned
the address is unknown.

## Candidate `court-headroom-001` (built, not installed)

| Area | Change |
|---|---|
| Wi-Fi buffers | Static RX 4 → 2 (SDK minimum) and dynamic RX 8 → 4 (one pinned TCP window: 5,760 B = 4 × 1,436 B MSS). CSI off. TX unchanged. Spec 04 §3 already says "limit buffers". |
| HTTP task | Stack 8 → 7 KiB. The upload/readback path left 3,680 B of 8 KiB unused (`court1-return-002`). Admission still reserves 8 KiB: `HTTP_START_FREE_HEAP` stays 57,344 B. **No floor or gate changed.** |
| Close | `TCP_NODELAY` on every session. The acknowledgement is published before `Closed`, so the loop cannot stop the server between them. Wi-Fi then stays up until the client closes the reply's connection (it has read the 200), at most 5 s (one retransmission). A controller connecting ends the wait at once. Serial reports `OTA close reply delivered=<0/1> after_ms=<n>`. |
| Telemetry | `OTA HEAP stage=` lines at `station-associated`, `station-ip-assigned` (the ESP32's DHCP server assigned an address), `request-info`, `request-login`, `request-status` and `request-close`. They are sampled where each happens and printed by the loop. Every `OTA HEAP` line also has `loop_stack=` (loopTask high-water mark, task 4.2). |
| CLI | A close whose request was sent but whose reply is lost is `close_reply_lost`. The CLI then still observes the link drop and runs BLE recovery, and reports `maintenanceClosure: verified_by_ble_recovery`; firmware resumes advertising only after teardown. A lost close without a BLE check still fails. |
| Windows | Rescan every 4 s until the BSS appears, for up to 40 s of a 70 s connect budget. The 30 s after discovery for association, DHCP, static fallback and TCP is unchanged. |

The bench-only `ota_budget` image, `court-headroom-diag-001`, adds a
`station-session` budget from the window opening to login. Each budget then
prints `OTA BUDGET-LOW` with:

- the allocation that set the low (size and six caller PCs; resolve with
  `xtensa-esp32-elf-addr2line -f -C -e firmware.elf`);
- the sizes of the eight allocations before it.

This names the owner of the join dip.

### Expected effect (inferred, to be measured)

- Network-ready heap up about 4 KB: two DMA RX buffers of about 1.6 KB [I],
  plus 1,024 B of HTTP stack.
- The dynamic RX cap bounds burst and upload RX buffering at four frames
  instead of eight.

None of this is measured yet. The heap gain alone does not clear the gate for
every session (55.5–61.1 KB measured).

| Static measure | `court-color-002` | `court-headroom-001` |
|---|---:|---:|
| OTA static DRAM | 62,460 B | 62,628 B (+168: milestones, close state) |
| esp32dev static DRAM | 39,612 B | 39,612 B |
| Image / slot headroom | 1,595,280 / 436,336 B | 1,596,512 / 435,104 B |

## Tests

| Suite | Result |
|---|---|
| Firmware Unity (native) | 104/104. New: the 7 KiB stack keeps the 8 KiB reserve and both floors; RX buffers hold one TCP window within SDK limits; the close flush waits for delivery within its bound, including `millis()` wrap |
| `esp32dev`, `ota`, `ota_b`, `ota_budget` builds | Pass |
| Scoring build audit; OTA build/rollback audit | Pass |
| Packaging tool image validation (both candidates) | Valid |
| CLI host tests | 88/88. New: lost close reply proven by link drop and BLE; lost reply without them fails; `request_sent` only after the request is sent; late AP found within one scan interval; absent AP stops at 40 s without associating |

## Next steps (owner approval needed for each)

1. Fresh Mode B isolation, then one USB application write of
   `court-headroom-diag-001` into `ota_0`, using the 2.2 procedure:
   - chunked 115,200-baud backup;
   - on-chip `verify_flash` before and after;
   - boot capture.
2. Serial-monitored, authenticated, no-write maintenance sessions with BLE
   recovery. Each gives the join dip owner, per-stage heap on the new buffers,
   whether DHCP ran, and whether the close reply arrived.
3. Only if the measured session heap clears the unchanged gate: one wireless
   write of signed `court-headroom-001` into `ota_1`, measuring the real upload
   transient. This replaces the portrait fallback.

## Artifacts

`candidates/<build>/` in the session folder:

| File | SHA-256 |
|---|---|
| `court-headroom-001/firmware.bin` | `657d8aaa342804b69906e0fc6cebbde0a53fda03cd943685b1858ba171b184fa` |
| `court-headroom-001/firmware.elf` | `c7be29358ebe7489603c64d2f50ed744ffc9cc2aabe8854047ee9e78dfa1190a` |
| `court-headroom-diag-001/firmware.bin` | `2b622561bdf47b8af1e0cad0953da1141a9214ae4b887482b3c7abbe6366387a` |
| `court-headroom-diag-001/firmware.elf` | `abde8b6feea97906c28c88b17c40e9c2d4b49daba039b77a7520c8cd36f74f8a` |

Neither image is signed. A wireless package needs the owner's key.
