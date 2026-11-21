# OTA reliability follow-up: evidence and first candidates

**Court 1 runs the bench build `court-headroom-diag-001` on `ota_0`.** It was
installed by one owner-approved Mode B USB application write and verified on
the chip. `ota_1` still holds portrait `court-controller-002`. The candidate
raised the session heap above the CLI gate. It also made every maintenance HTTP
exchange take seconds, so it is not the release. `court-headroom-002`, built
but not installed, targets that regression.

Change: `openspec/changes/ota-reliability-followup/`. Evidence:
`_AgentArtifacts/pickleball_scoreboard/sessions/20261121-claude-ota-followup/`,
which also reanalyses the 2.2 logs in `20261121-claude-brightness/v22/`.

## Hardware results (`court-headroom-diag-001`)

### USB install (authorized once, consumed)

- **Identity:** COM11 CH340, ESP32-D0WD-V3 rev 3.1, MAC `3c:8a:1f:a1:ba:28`,
  4 MB flash.
- **Backup:**
  - Read in 16 chunks of 256 KiB at 115,200 baud. Even at that speed the CH340
    dropped bytes: chunk 1 needed all 4 attempts, and chunks 2, 12 and 14
    needed 2.
  - `verify_flash 0`: `digest matched`. SHA-256 `f4a81b32…`, the image
    expected after the 2.2 `court-color-002` install, so nothing had changed.
  - Before state: otadata seq 9 VALID on `ota_0`; `ota_0` = `court-color-002`
    (`a71823b7…`); `ota_1` = `court-controller-002` (`1707f73e…`).
- **Write:** all seven pre-write gates passed. App-only `write_flash` at
  0x10000; erase `[0x10000, 0x197000)`; esptool verified the data hash.
- **After:** `verify_flash 0` against the backup with only that region
  replaced (`48da5a40…`): `digest matched`. Bootloader, partition table, NVS,
  otadata, `ota_1` and coredump are byte-identical to before.
- **Boot:** `OTA build=court-headroom-diag-001 … slot=ota_0 pending=0`.
- **Serial:** one capture covers the boot and three sessions. It contains no
  panic, reset, watchdog or brownout.

### Heap

| Measure | `court-color-002` | `court-headroom-diag-001` |
|---|---:|---:|
| Wi-Fi init cost | 31,488 B | 25,776 B |
| HTTP start cost | 12,672 B | 11,648 B |
| Network-ready, boot | 65,428 B | **71,288 B** |
| Network-ready, BLE-activated | 63,668–63,728 B | 69,524–69,544 B |
| Session admitted | 55,512–60,320 B | 65,240 B (one session) |
| Authenticated status heap (CLI gate input) | 56,980–61,076 B | **64,336 B** (gate 60,456) |
| Lowest heap in window or session | — | 48,352 / 45,620 / 44,116 B: 22.9–25.4 KB below network-ready |
| HTTP stack free on the status path | 5,512 B of 8 KiB | 4,412 B of 7 KiB |
| `loopTask` stack free (task 4.2) | not logged | 4,680 B of 8 KiB at every stage |

Two of the three changes did what was planned; the third did not:

- The buffers and the stack gave **+5.8 KB** at network-ready, and the one full
  session cleared the gate by 3.9 KB.
- The join dip did not shrink. The allocations around each low repeat
  1,696 B, 24 B and 16 B, the last one in lwIP's pool. The bench build only
  recorded the PCs of the final 16 B allocation, so the 1,696 B owner is still
  unknown.

### HTTP latency regression

| Session | Activation | Probe timeout | Result |
|---|---|---|---|
| S0 | Boot window, advertising on | 3 s | Both `/api/info` probes timed out |
| S1 | BLE | 3 s | Both `/api/info` probes timed out |
| S2 | BLE | 10 s (experimental CLI copy outside the repo) | Every request answered, in **3.6–9.6 s**, including after login with advertising stopped |

In 2.2 the same `/api/info` probe answered within 3 s in 5 of 7 sessions.

What S0–S2 show:

- **Delays in both directions.** Solving S2's device arrival times against the
  CLI durations: requests reached the device 1.7–6 s late (TCP connects took
  about 2 s), and replies took a further 2.5 s or more. This is frame loss on
  the link with recovery by retransmission, not slow handlers.
- **Not a stale cached lease.** In S0 the ESP32's DHCP server assigned the
  address (twice), and the `/api/info` handler ran.
- **The close reply now arrives.** The CLI got the 200 after 4.2 s. The device
  saw no client FIN within the 5 s bound (`delivered=0 after_ms=5002`) and then
  stopped.
- **Scoring BLE was unaffected:** initial sync passed after every session.

The candidate changed three things on this path: static RX buffers 4 → 2,
dynamic RX 8 → 4 with CSI off, and `TCP_NODELAY` (about eight frames per reply
instead of two). Any of them could cause this.

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

## Candidate `court-headroom-001` (its bench build is installed)

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

### Expected effect, before measuring

- Network-ready heap up about 4 KB: two DMA RX buffers of about 1.6 KB [I],
  plus 1,024 B of HTTP stack.
- The dynamic RX cap bounds burst and upload RX buffering at four frames
  instead of eight.

Measured on Court 1 (above): +5.8 KB at network-ready. The dip did not shrink,
and HTTP slowed to seconds per request.

| Static measure | `court-color-002` | `court-headroom-001` |
|---|---:|---:|
| OTA static DRAM | 62,460 B | 62,628 B (+168: milestones, close state) |
| esp32dev static DRAM | 39,612 B | 39,612 B |
| Image / slot headroom | 1,595,280 / 436,336 B | 1,596,512 / 435,104 B |

## Tests

| Suite | Result |
|---|---|
| Firmware Unity (native) | 104/104. New: the 7 KiB stack keeps the 8 KiB reserve and both floors; RX buffers hold one TCP window within SDK limits; the close flush waits for delivery within its bound, including `millis()` wrap |
| `esp32dev`, `ota`, `ota_b`, `ota_budget` builds (`-001` and `-002`) | Pass |
| Scoring build audit; OTA build/rollback audit | Pass |
| Packaging tool image validation (all four candidates) | Valid |
| CLI host tests | 96/96. New: lost close reply proven by link drop and BLE; lost reply without them fails; `request_sent` only after the request is sent; late AP found within one scan interval; absent AP stops at 40 s without associating; timings carry no headers; `tools/flash_backup.py` (task 6.1: chunk retry, 64 KiB fallback, abort without a backup, on-chip digest required, exact expected image, isolation and baud refusals) |

## Candidate `court-headroom-002` (built, not installed)

| Area | Change from `-001` |
|---|---|
| Replies | `TCP_NODELAY` is gone. An API reply is one `httpd_send` of the status line, headers and JSON body: one segment, so there is no Nagle wait and one retransmission at most. The page and script keep `httpd_resp_send`. |
| Bench matrix (`court-headroom-diag-002` only) | Each activation this boot takes the next Wi-Fi profile, `{static RX, dynamic RX, CSI}`: `{4,8,on}` (2.2) → `{2,8,on}` → `{4,4,on}` → `{4,8,off}` → `{2,4,off}`, then repeats. Serial prints `OTA DIAG wifi profile=`. One install then measures each knob's heap and latency. |
| Bench attribution | Each new budget low records 10 caller PCs, enough to get past the allocator's four frames. It also records the PCs of the most recent allocation of at least 1 KiB (`OTA BUDGET-LARGE-PC`), the suspected 1,696 B owner. |
| CLI | The report gains `httpTimings`: method, path, status or failure stage, and seconds; never headers or bodies. The public `/api/info` probe waits 5 s, longer than one 3 s retransmission. |

Production `court-headroom-002` keeps `{2,4,off}` and the 7 KiB stack until the
matrix says otherwise. Static DRAM is unchanged at 62,628 B (`ota`).

## Next steps (owner approval needed for each)

1. Fresh Mode B, then one USB application write of `court-headroom-diag-002`
   into `ota_0`, using the same procedure as `-001`.
2. About ten serial-monitored, no-write sessions: two per profile. Choose the
   production profile from latency, heap and the large-allocation owner.
3. Only with a profile that is fast and clears the unchanged gate: one wireless
   write of signed production firmware into `ota_1`, measuring the real upload
   transient (task 1.3). This replaces the portrait fallback.

## Artifacts

`candidates/<build>/` in the session folder:

| File | SHA-256 |
|---|---|
| `court-headroom-001/firmware.bin` | `657d8aaa342804b69906e0fc6cebbde0a53fda03cd943685b1858ba171b184fa` |
| `court-headroom-001/firmware.elf` | `c7be29358ebe7489603c64d2f50ed744ffc9cc2aabe8854047ee9e78dfa1190a` |
| `court-headroom-diag-001/firmware.bin` | `2b622561bdf47b8af1e0cad0953da1141a9214ae4b887482b3c7abbe6366387a` |
| `court-headroom-diag-001/firmware.elf` | `abde8b6feea97906c28c88b17c40e9c2d4b49daba039b77a7520c8cd36f74f8a` |
| `court-headroom-002/firmware.bin` | `be4e63a1445081ea34399783c663398543068e62d830e3cceae7c5d13c071b76` |
| `court-headroom-002/firmware.elf` | `f1923e900f15b2331d246ffc6a0b447087e94875d18baadf023986ec526aa3d5` |
| `court-headroom-diag-002/firmware.bin` | `ff4a8a680b47cd36c6429d3a540a868b6b1b84a21f08e55d06e4509ef26f8461` |
| `court-headroom-diag-002/firmware.elf` | `6ba760d3b015e537e321166b68e08b5d813a85fc7ee020d44f81d282a157a192` |

No image is signed. A wireless package needs the owner's key.

Hardware evidence:

- `usb-install/`: identity, backup log, `analysis-before.txt`, `gates.txt`, the
  write and verify logs, `serial.log` and `serial-summary.txt`.
- `sessions/s0-boot`, `s1` and `s2-exp10s`: CLI reports and BLE checks.

`court1-before.bin` contains private NVS and stays in the session folder, never
in Git.
