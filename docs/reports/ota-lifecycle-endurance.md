# OTA lifecycle endurance and first-update readiness

## Verdict

**Not lifecycle-cleared; the definition of done is not yet met.** The installed `agent-wpa2` completed50 recorded authenticated maintenance cycles, but available heap continued to decline and did not recover after five quiet minutes. One real HTTP-login transport failure also remains recorded. Do not relabel this as50 uninterrupted successful cycles, a proven harmless warning, or a production-ready `agent-ota1` deployment.

No USB reset/flashing, erase, verified-image staging, install, metadata/bootloader/table/eFuse change or warning suppression was performed. The real upload endpoint was exercised only with invalid/incomplete requests proven to stop before flash begin. The installed application remains `agent-wpa2`/`ota_0`; WPA diagnosis was not reopened.

A small **source-only diagnostic candidate `agent-life1`** is signed, link-audited and dry-run verified. It has not been installed and is **not represented as a leak fix**. A first controlled Wi-Fi diagnostic installation requires explicit owner authorization before further attribution can be measured.

## What was actually tested

Protected session:

```text
D:\Documents\tryPython\_AgentArtifacts\pickleball_scoreboard\sessions\20261118-pi-lifecycle-stability\
```

The harness passively opened COM11 with DTR/RTS inactive; it never entered the ROM loader, reset the ESP32 or wrote serial commands. BLE target-bound HMAC activation and pinned HTTP image identity established the actual installed target. Captures show no reset/panic/canary/watchdog/admission rejection; firmware uptime increased across all recorded runs.

Each cycle used the same sequence: authenticated BLE activation/disconnection →5s after AP-ready → Native WLAN/DHCP → pinned info/admin login →5s authenticated settling → status → HTTP close → Windows restoration → cleanup completion +8s. Windows adapter-wide mutex ownership prevented competing agent sessions. Ethernet-source TLS probes passed before, at regular cycle checkpoints, during load probes and after. Wi-Fi returned to its actual disconnected/software-on entry state.

- First run:22 complete cycles; attempt23 failed at HTTP login transport, after WLAN and info had succeeded. It was aborted/restored, not replayed or counted as success. The original exception lacked a nested socket cause, so no unsupported root cause is assigned.
- Continuation:22 additional complete records. Its next actual login/status/close/restoration succeeded, but a literal serial-prefix check failed because concurrent SDK output split `OTA` into `O...TA`. That failed runner/result is retained; the extra physical cycle is independently recognizable but excluded from the principal50-row dataset.
- Final run:6 complete records. Thus **50 complete recorded cycles plus one extra raw-confirmed cycle**, one transport failure, and one logger-parser failure—not an uninterrupted50-cycle pass.
- Only the exact known warning is removed in a derived parser view; untouched raw logs retain interleaving and all error messages. Numeric `cleanup stack free=` labels provide stack evidence without assuming the `OTA` prefix is indivisible.
- The end-of-test AP timeout again closed after **179.96s**. Actual BLE notifications then returned `0,0,L,2,0,D` → `1,0,L,2,0,D` → `0,0,L,2,0,D`. Maintenance was left closed, BLE available, COM11 released.

## Memory results

All values are bytes. Full stage data are in `cycles-checkpoints.csv`; historical minima and largest INTERNAL|8BIT blocks were retained for each observable stage. Fields are sequential, not atomic cross-task snapshots.

| Consistent checkpoint | First | Last | Difference |
|---|---:|---:|---:|
| Pre-network, after previous cleanup/quiet interval and the same BLE authorization sequence |120,420 |116,364 |−4,056 |
| AP-ready |66,048 |62,024 |−4,024 |
| Authenticated,5s after login |63,444 |59,660 |−3,784 |
| Immediate network-closed telemetry |116,596 |112,948 |−3,648 |

**Immediate closed telemetry is not called a settled checkpoint.** The next pre-network measurement follows the fixed quiet delay and identical activation sequence; the new source candidate adds a true loop-owned closed-settled measurement.

Ten-cycle medians:

| Checkpoint |1–10 |11–20 |21–30 |31–40 |41–50 |
|---|---:|---:|---:|---:|---:|
| Pre-network |119,364 |118,932 |118,196 |117,566 |116,884 |
| Authenticated settled |62,780 |62,178 |61,420 |60,776 |60,044 |
| Largest ready INTERNAL8 block |38,900 |38,900 |38,900 |38,900 |38,900 |

Last30-record pre-network slope: **−68.89 bytes per completed-record index**. Excluded/interrupted windows still exercised the hardware, so this descriptive regression is not an exact allocation-loss-per-activation measurement. A120s closed pause before cycle11 partially recovered memory but did not remove the trend. After the full run, **300s closed quiescence** yielded pre-network116,492: only128 bytes above the previous116,364, still3,928 below the initial120,420. The pinned TCP MSL is60s; this longer pause is not consistent with a short TCP-close delay being the whole explanation.

Historical INTERNAL minimum across stage samples reached39,756; minimum recorded cleanup-stack margin was2,252 bytes. Full results are preserved in `cycles-analysis.json`. Original100,000/49,152/32,768 floors and contiguous-block checks were never weakened or bypassed. No allocation/admission failure was observed. One authenticated sample dipped to53,532 with later recovery, illustrating why individual timestamps alone are insufficient.

**Evidence-supported classification:** persistent loss of free capacity, not a demonstrated warm-up plateau or proven unbounded leak. Largest-block medians stopped shrinking while total free bytes declined, but those values alone cannot distinguish fragmented smaller pools, live allocations, retained caches and allocator overhead. No allocation owner is proven; do not invent a confirmed application leak or label this leak-free.

The installed SDK has `CONFIG_HEAP_TRACING_OFF=1`; timer/event profiling is off. Its HTTP interface exposes no allocated-block/task/timer counts or tracing control. Such counts were **not measured or inferred as actual live counts** in this run. Enabling them on the device would require another firmware installation, which this task does not authorize.

## Lifecycle and warning trace

The actual application path is:

1. Loop accepts a fresh BLE authorization only after requester disconnect, no intervening scoring activity, cleanup complete and bounded cooldown. Credentials are read-only; timers in `ota::Policy` are unsigned-millis comparisons, not SDK timer objects.
2. Netif/lwIP init → owned default event loop if needed → custom Wi-Fi event registration → AP netif/default handlers → bounded Wi-Fi buffer configuration/init → DHCP/AP mode/config/start →8KB HTTP task/two sockets/routes → unchanged admission checks.
3. Authenticated session stops BLE advertising and waits for the acknowledged quiet state; BLE controller itself is never deinitialized/recreated.
4. Closure stops/joins HTTP handlers before transfer cancellation, waits for AP_STOP after `esp_wifi_stop`, calls `esp_wifi_deinit`, unregisters the custom handler, destroys default Wi-Fi netif/handlers, deletes only the owned event loop, clears session/PSK and resumes ordinary BLE. Destruction runs on the existing4KB static cleanup task, not the scoring loop.

Pinned IDF4.4.7 source and the actual installed ELF were inspected. `httpd_stop` waits for its server task to reach STOPPED, closes sessions and frees server/URI/session storage. Event-loop deletion frees registered contexts and queued event data; default-netif destruction frees its driver/configuration storage. No obvious unmatched application-owned allocation was established by inspection. lwIP and some framework resources are intentionally process-lived.

The existing warning is in the **installed ELF's `ieee80211_timer_process`** guard: after acquiring the API lock, non-initialized Wi-Fi state causes unlock/log/return. The warning branch has no explicit timed wait. Thus the string does not itself prove `esp_wifi_deinit` timed out; actual stop/AP_STOP/deinit checks remained successful. Numeric types4/5 remain vendor dispatch IDs, not invented AP/WPA failure reasons. No generic online teardown workaround was applied.

A further pinned-source **candidate mechanism** was found and checked against actual disassembly: `ets_timer_done` calls `esp_timer_delete`, ignores its return and clears the stored handle/magic; `esp_timer_delete` returns `ESP_ERR_INVALID_STATE` for an armed timer. That path could orphan an allocation **if reached with an armed timer**. The current logs do not prove it is reached or that it explains the measured growth. Late queued callbacks can also warn after an otherwise successful deletion. No disarm/reorder workaround was installed or presented as a verified fix.

The warning therefore remains **SDK-internal late-dispatch behavior with unresolved lifecycle/resource attribution**, not safely dismissed as harmless given the observed heap trend and HTTP failure.

## Real maintenance load safety without erasing flash

Two added host tests use the actual `ota::Transfer` implementation: all partial valid prefixes0–23 bytes followed by cancellation, and a complete1024-byte invalid prefix buffer, must leave Sink.begin/writes/selects at zero. The full88-test MSVC suite passed before hardware probes.

Then actual authenticated `/api/upload` requests tested:

| Case | Actual status |
|---|---:|
| Wrong origin / wrong nonce |403 /403 |
| Wrong content type |400 |
| Undersized body |413 |
| Invalid package magic |422 |
| Altered signature |422 |
| Genuine owner-signed header + invalid ESP32 prefix |422 |
| Interrupted header / interrupted payload after trusted header |408 /408 |

The maximum actual body was the170-byte header plus1024 **zero** bytes; interrupted payload sent only one image-magic byte. Declared lengths matched the signed header where needed, but **no complete valid24-byte image prefix was transmitted**. The real handler verified ECDSA before rejecting the invalid/incomplete image; it never reached FlashSink.begin. Raw capture has no before/after-flash-begin, staged or installing marker. Every request recovered to the authenticated Session state with zero published image bytes/no staged build. Running image hash/slot/boot state remained unchanged.

Signature-stage current INTERNAL values were57,264–57,328, with largest internal byte block at least28,660; historical minimum remained39,756. These are real MCU crypto/HTTP observations, not OpenSSL simulation. They do **not** prove full erase/write/readback/transfer-load or HTTP-task stack safety; those stages are deliberately untested because valid firmware staging would erase/write inactive flash.

## Surgical changes and verification

No speculative lifecycle behavior fix was applied. The proposed `agent-life1` source adds only:

- allocator allocated-byte/block/free-block and task counts to bounded stage telemetry;
- one quiet `closed-settled` sample5s after cleanup, only when BLE is disconnected;
- return-preserving observation of linked public `esp_timer_delete` calls: success, armed/invalid-state rejection, other error and raw rejected caller. It neither cancels timers nor changes return codes, SDK libraries or log levels. These are delete-call counts, **not globally live timer counts**.

It adds24 bytes static data/BSS; the build audit reports110,746 instruction-RAM bytes. Formatting buffer grows256→512 stack bytes; net runtime heap/stack effects require installed measurement. Existing8KB HTTP/4KB cleanup reservations and admission policy remain unchanged. Source-only diagnostics are not retroactively claimed as measurements on `agent-wpa2`.

Actual ELF link audit proves ETS done calls the observer, which calls the real delete routine. A host test executes the actual wrapper body with a fake SDK callee: all four tested return codes/argument/single-call semantics are preserved; not claimed as MCU validation.

Regressions: **88 MSVC Unity**, **29 CLI**, **15 real signing/package**, **42 forced-rerun Android JUnit**; glyph/layout checks and48 actual C++ snapshots pass. `ota`, `ota_b`, non-OTA builds and unchanged partition/bootloader/strong deferred-confirmation audits pass. Earlier active C++ LSP probes timed out and were not treated as clean; the final active probe completed clean for both changed C++ files. The eight-path final check found only three dictionary false positives on verified MAC octets, marked as such without source suppression.

## Packages, recovery and exact pending command

Frozen `agent-ota1` signature/target/ABI/version/hash verified again; CLI preflight and its deploy **dry-run** pass with no activation/transfer. It remains1,589,824 bytes, image0249feffc99be29b57744b5c10298e4a1de923ef63c9d3bf212288a097867da5, package2185fe3a37999b81c89ff5f19d49a8a54ac714cb5cb9fcd16de836d0695d39a3. **It does not contain the new diagnostics and is not lifecycle-cleared.**

Signed diagnostic **`agent-life1`**,1,590,368 bytes/441,248 slot headroom:

- Image **`c94cea49e564da6c0d94dfcca7c75f4c1c55ad16522af95f9c2d9944e202d86d`**.
- Package **`293964d00c61809ab201be87e69b45b38ba9067f7570393a31fb6277ea0949e6`**.
- Owner-key verification and deploy dry-run pass; **not uploaded, staged or installed**.

Last tested running slot was ota_0; the validated existing layout/`validSlots` check makes expected inactive target **ota_1 at0x200000, size0x1f0000**. Both current4MB recovery copies freshly hash to181dcc684f706a7021cd7e39c31f2ccb226ae050b35056bbd4de8243a7a76b2f; no new physical full read/reset was performed in this session. All older recovery artifacts remain intact.

After approval, first re-read the actual target/build/slot/hash/health and stop on any mismatch from the last tested agent-wpa2/ota_0/image db32e3d046acdfb775797d0d504cb0074445d6bb6ead7171805feb91f0555dea. No hardware was accessed merely to finish the resumed handoff.

**Do not execute without new explicit owner approval** for a controlled diagnostic first Wi-Fi update, volatile-score reset, inactive ota_1 erase/write (overwriting the original production fallback) and normal OTA metadata selection/trial/confirmation writes:

```bash
firmware/.venv/Scripts/python.exe tools/ota_deploy.py deploy --device court1 --package "D:/Documents/tryPython/_AgentArtifacts/pickleball_scoreboard/sessions/20261118-pi-lifecycle-stability/diagnostic-candidate/agent-life1.pksota" --authorize-score-reset --output "D:/Documents/tryPython/_AgentArtifacts/pickleball_scoreboard/sessions/<new-authorized-ota-session>"
```

Expected: agent-wpa2/ota_0 → signed stream/readback verified in inactive ota_1 → explicit install/NEW metadata → ota_1 pending boot → startup checks +5s/100 healthy loops → VALID/confirmed/healthy with exact expected whole-image hash → restored Windows Wi-Fi/Ethernet. The CLI must reject wrong version/slot/hash/unconfirmed state and never replay an uncertain install POST or silently use USB.

This first diagnostic installation is the **remaining required authorization** to expose allocation/timer outcomes and identify/fix the resource growth. It is not approval already granted, a production-readiness claim, an executed update or a promise the diagnostic build cures the issue. Actual rollback execution, illuminated panels and complete firmware transfer/load remain unverified. No push/merge.
