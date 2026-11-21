# OTA reliability follow-up: evidence and candidates

**Court 1 runs production candidate `court-headroom-003` on `ota_0`.**

- **Install:** a third owner-approved Mode B USB application write, verified
  on the chip.
- **Wireless write:** the one approved write of signed `court-headroom-003b`
  into `ota_1` cleared the heap gate. It then ran out of the 120 s transfer
  window on a lossy link, and the device refused it.
- **Fallback lost:** `ota_1` now holds a partial, invalid image instead of the
  portrait fallback (replacing it was approved). `ota_0` is valid and running.
- **Health after:** scoring BLE passed, and the serial log has no faults.

**Heap is no longer what blocks wireless updates; link quality is.**
`court-headroom-004`, built but not installed, picks the quietest AP channel
and gives Wi-Fi priority during a session.

## Production install and wireless test (`court-headroom-003`)

### USB install (third authorization, consumed)

- **Before:** on-chip `verify_flash` matched `8d13776a…`, the image expected
  after the `-diag-002` install.
- **Gates and write:** all seven gates passed; app-only write at 0x10000.
- **After:** `verify_flash` matched `94008126…`.
- **Boot:** `OTA build=court-headroom-003 … slot=ota_0 pending=0`.
  Network-ready heap 68,660 B (2.2: 65,428 B). Wi-Fi init 29,040 B (2.2:
  31,488 B).

### Sessions

| Session | Result |
|---|---|
| b0, boot window | `/api/info` replies lost |
| b1 | AP first seen at 29 s; no TCP connection for 70 s |
| b2 | `/api/info` replies lost |
| b3 | Complete: status heap 63,156 B, close acknowledged and delivered |

In b2 the ESP32's DHCP server assigned the address 6 times. Windows kept
retrying because the device's replies were not reaching it.

### Wireless deploy of signed `court-headroom-003b`

The package (`78d30c0f…`, image `a5a05e25…`) was built and signed by the CLI.
It is the same commit as `-003`; only the build ID differs.

| Stage | Heap |
|---|---:|
| Status before upload | 64,528 B (gate 60,456 B) |
| After signature | 58,732 B |
| After flash-begin | 58,468 B |
| Global minimum during the upload | 48,248 B, unchanged |

- **Upload cost (task 1.3):** the global minimum was set earlier, at a
  station join, and the upload never lowered it. So this upload cost at most
  16,280 B below the status heap, against the gate's assumed 24,616 B. The
  transfer did not complete, so this bounds only the part that ran.
- **Refused:** after 120 s the transfer window expired, and the device
  refused with 422 and `inactive_written=1`. Earlier wireless uploads took
  20 s.
- **Aftermath:** the CLI's status and close requests timed out. The device
  closed the session itself at the 600 s limit (cleanup `ESP_OK`). BLE then
  synced 2/2.

### The link, not the firmware, is what varies

| Factor | Observation |
|---|---|
| Signal | −29 to −53 dBm in good and bad sessions alike |
| Time | Identical configurations mostly worked at 09:50–10:30 and 12:24–12:35 UTC, and mostly failed at 11:50 and 14:26–14:35 |
| Channel | A strong neighbour (84%) on the AP's fixed channel 1; channels 6 and 11 empty at the desk |
| Windows | On AC power, so Wi-Fi power saving is set to Maximum Performance |

Requests also stay slow after login, with BLE advertising off.

## Candidate `court-headroom-004` (built, not installed)

| Change | Why |
|---|---|
| Before the AP starts, one active scan (about 1 s) picks the least occupied of channels 1, 6 and 11 (`ota::quietestChannel`). A neighbour counts against every channel it overlaps, weighted by signal; a tie or any failure keeps channel 1. Serial prints `OTA AP channel=`. | Spec 04 does not fix the channel. Channel 1 had the strong neighbour. The CLI pins the BSSID, not the channel. |
| `esp_coex_preference_set(WIFI)` from session admission, `BALANCE` restored before advertising resumes. | BLE is already paused for the session (Spec 04 §3), so gameplay keeps priority. |

Static DRAM 62,636 B (+8). Image `38ed911f…`, 1,599,728 B.

A ten-session matrix on that build settled the Wi-Fi configuration:

- Two DMA receive buffers drop connections.
- CSI off frees about 2.4 KB without harm.
- The join dip is Wi-Fi receive buffers queued for the TCP/IP thread.

The resulting production candidate `court-headroom-003` is built, not installed.

Change: `openspec/changes/ota-reliability-followup/`. Evidence:
`_AgentArtifacts/pickleball_scoreboard/sessions/20261121-claude-ota-followup/`,
which also reanalyses the 2.2 logs in `20261121-claude-brightness/v22/`.

## Profile matrix (`court-headroom-diag-002`)

Each activation used the next `{static RX, dynamic RX, CSI}` profile. Every
build here sends each API reply as one write and waits for the client to close
the reply's connection. a0 used the boot window; a1–a9 used BLE activation.

| Profile | Wi-Fi init cost | Network-ready | Status heap | Sessions OK | Median request time per session |
|---|---:|---:|---:|---:|---|
| 4/8/on (2.2) | 31,116–31,488 B | 63,644–65,372 B | 62,428, 60,816 B | 2/2 | 3.0, 3.5 s |
| **2**/8/on | 26,620 B | 68,140 B | 64,464 B | **1/2** | 3.8 s |
| 4/**4**/on | 31,116 B | 63,644 B | 60,676, 59,844 B | 2/2 | 2.7, 2.0 s |
| 4/8/**off** | 28,664–28,668 B | 66,024–66,088 B | **62,668, 63,048 B** | 2/2 | **1.7, 1.1 s** |
| **2**/4/off | 25,396–25,420 B | 69,252–69,360 B | 65,800 B | **1/2** | 3.5 s |

**Static RX 2 breaks the link.** Both failed sessions (a4, a6) were
static-2 profiles. In each, the station associated and the ESP32's DHCP
server assigned the address. Then for 70 s no TCP connection reached the
server, and no handler ran. Static-4 profiles completed 6 of 6.

**The other two changes:**

- **CSI off** saves 2.4 KB at init and had the best latencies.
- **A dynamic RX cap of 4** saves nothing at network-ready.

Requests still take one to seven seconds in every profile, including after
login with advertising stopped. 2.2 never measured this, so it is not shown to
be a regression of this work. I have not yet found what causes it.

**Join dip owner (task 1.1).** The bench sampler's large-allocation PCs resolve
to `wDev_ProcessRxSucData → wDev_IndicateFrame → esf_buf_alloc →
esf_buf_alloc_dynamic → wifi_malloc`. Each such allocation is followed by
lwIP's `tcpip_inpkt` message (24 B) and a pbuf (16 B).

- These are the Wi-Fi driver's receive buffers: 2,308 B each with four static
  buffers, 1,696 B with two.
- They are frames queued for the TCP/IP thread when the station joins. The
  dynamic-RX cap does not bound frames already handed to lwIP.
- The dip measured 12.9–20.3 KB below network-ready in this matrix, against
  22.9–25.6 KB before.

**Close (task 2.2).** All 8 sessions that reached close got the 200; 2.2 got
it in 1 of 2 and `court-color-001` in 0 of 4. The device saw the client's FIN
within the 5 s bound in 4 of 8; the other 4 replies were still delivered. Two
more acknowledged closes are needed for 10/10.

**Evidence:** `usb-install-002/` (identity, before and after verify, gates,
write log, `serial.log`, `matrix.txt`) and `sessions-002/a0-boot … a9`. The
before image was verified on the chip against the file built for the `-001`
install (`48da5a40…`). The after image is `8d13776a…`.

## Candidate `court-headroom-003` (built, not installed)

`-002` with static RX 4 and dynamic RX 8 (both as in 2.2), CSI off, and no
bench matrix. Its Wi-Fi config and every other setting match profile 4/8/off;
only the bench sampler differs. Expected status heap: 62.7–63.0 KB, 2.2–2.6 KB
above the unchanged gate. That margin is thin, so the next candidates for
headroom are the HUB75 colour depth (owner decision, task 1.4) and an upload
transient measurement on this build (task 1.3).

## Earlier hardware results (`court-headroom-diag-001`)

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

Production `court-headroom-002` kept `{2,4,off}`; the matrix replaced it with
`-003`. Static DRAM is unchanged at 62,628 B (`ota`).

## Next steps (owner approval needed for each)

Steps 1 and 2 of the previous list are done (above).

1. One USB application write of `court-headroom-004` into `ota_0`, then
   no-write sessions. Check `OTA AP channel=` (6 or 11 expected at the desk),
   request latency, and two more acknowledged closes for 10/10.
2. One wireless write of signed `court-headroom-004b` into `ota_1`, which now
   holds an invalid partial image, so no fallback is lost. This measures the
   complete upload transient (task 1.3) and gives a valid fallback again. Run
   it at the court rather than at the desk if the desk link stays poor.
3. HUB75 colour depth (task 1.4):
   - The pinned library fixes the depth at compile time, with CIE tables only
     for 4, 6, 7, 8, 10 and 12 bits, so 3 bits is not available.
   - The display costs 21,028 B of heap at 8 bits. 4 bits should free roughly
     8–10 KB [I].
   - The palette uses only full channels, so the image should be identical,
     but refresh and OE timing change. This needs a bench build and the owner's
     lit-panel acceptance before any install.
4. If links stay slow on clear channels, capture a packet trace in an
   elevated session to separate client, air and device loss (task 2.1).

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
| `court-headroom-003/firmware.bin` | `83e210cf49643df9ae1ca9f95250511ec7761c1114fe8445fbfa3d2e75ccf3ca` |
| `court-headroom-003/firmware.elf` | `ecd1463fcb4b7b8c29531df8c2b715646565b8647ea7fdd23297844a06d995ad` |
| `court-headroom-diag-003/firmware.bin` | `15c671d5146214fce53206774e487ec15543f15e99ee4f3f502dd39de848eede` |
| `court-headroom-diag-003/firmware.elf` | `e992f259e76d68463ab3ef53ab0c967b0da1746604d04236c04059cc2ef40fca` |
| `package-003b-2/court-headroom-003b.pksota` (signed, used once) | `78d30c0fe9d60ac1abff2476ef9eaa0ea0620201579127f3822a851a3853d1b6` |
| `court-headroom-004/firmware.bin` | `38ed911f9c43d7b5297b3c132b999399446cd16701beb6d17c8190a9f3093629` |
| `court-headroom-004/firmware.elf` | `bd17f703fe29001e761a0bf5afa9e59392c2aadf14505e7c895ef6930672bc12` |

No image is signed. A wireless package needs the owner's key.

Hardware evidence:

- `usb-install/` (`-diag-001`): identity, backup log, `analysis-before.txt`,
  `gates.txt`, the write and verify logs, `serial.log` and
  `serial-summary.txt`.
- `sessions/s0-boot`, `s1` and `s2-exp10s`: CLI reports and BLE checks.
- `usb-install-002/` and `sessions-002/` (`-diag-002`): the same records,
  plus `matrix.txt` and `analyze_matrix.py`.

`court1-before.bin` contains private NVS and stays in the session folder, never
in Git.
