# agent-wpa2 — authorized installation and actual wireless connectivity

## Verdict

**The startup heap-admission failure is resolved on this board without lowering its safety thresholds. Windows WPA2/CCMP, DHCP and authenticated HTTP now pass on actual hardware, four times.** This is not a wireless firmware-update, transfer-load, leak-free endurance, physical-panel or rollback-execution pass.

The SDK shutdown warning persists. Small downward heap drift remains unexplained; do not claim all memory/lifecycle problems are resolved or seal the unit. The original failed runners remain preserved and are explained below.

## Authorization, exact installation and recovery

The owner freshly confirmed both panels unpowered, HUB75 disconnected while unpowered, external5V absent and ESP32 laptopUSB-only; explicitly authorized only the signed `agent-wpa2` application and volatile-score reset. Source: `42d9253052fecd66f7f87738e303da96fa0e82e4`, branch `feat/agent-wireless-deployment`.

Fresh identity: **COM11**, CH340/location1-1, **ESP32-D0WD-V3 rev3.1**, MAC **3c:8a:1f:a1:ba:28**,40MHz,4MB/JEDEC0x162085. Secure Boot/encryption remain disabled, crypt count0. The actual whole-flash comparison established the existing NVS/table/metadata/application/fallback state, not just the repository partition CSV.

- Preserved pre-wpa1 full recovery plus the exact previously verified wpa1 payload and erased padding transparently reconstructed expected current wpa1. **Actual whole-device digest AND independent standard4MB verification matched before writing**. This is not misrepresented as a new physical full read. Two protected copies: SHA256 `e971c7aaa4ad07b10b0c07464caa14e66db0a1983fe1d5ad7e49b549d92c1074`.
- Independently checked existing owner-public-key signature and exact package payload. Installed **1,589,824 bytes at0x10000 (`ota_0`)**, sector interval **[0x10000,0x195000)**. Image SHA256 `db32e3d046acdfb775797d0d504cb0074445d6bb6ead7171805feb91f0555dea`; package SHA256 `726af4d7f8aa6de687076af63c27a8454ae6f8e8117177a354801d2397a7c5b3`.
- Upload hash, independent app verification, and **all other sectors [0,0x10000) + [0x195000,0x400000)** passed. No bootloader/table/metadata/NVS/fallback write, migration, provisioning, erase-all or eFuse/security operation was issued.
- After runtime testing, reconstructed current wpa2 recovery was again matched to the **actual entire device by digest AND standard4MB verification**. Two protected current copies: SHA256 **`181dcc684f706a7021cd7e39c31f2ccb226ae050b35056bbd4de8243a7a76b2f`**. Every unrelated region, including OTA metadata and private NVS, still matches the original pre-write state.

Existing partition layout remains NVS0x9000/0x5000, metadata0xe000/0x2000, ota_0 at0x10000/0x1f0000, ota_1 at0x200000/0x1f0000, coredump0x3f0000/0x10000. All older recoveries remain untouched. Full images contain private NVS: never publish them.

## Actual cold-start heap measurements

Bytes; `free`/`min` use INTERNAL capability, while the byte/block columns explicitly use INTERNAL|8BIT. Fields are sequential samples, not a cross-task atomic snapshot.

| Stage | Current INTERNAL | Historical minimum INTERNAL | Current INTERNAL 8-bit | Largest INTERNAL 8-bit block |
|---|---:|---:|---:|---:|
| Before BLE controller |199,084 |192,652 |179,492 |110,580 |
| After Classic RAM release |214,052 |207,672 |194,460 |110,580 |
| After enabling BLE controller |195,980 |193,640 |176,388 |110,580 |
| After BLE/GATT service |127,792 |127,688 |108,200 |106,484 |
| Before networking |127,544 |127,124 |107,952 |106,484 |
| After AP netif |117,900 |117,796 |98,308 |94,196 |
| After Wi-Fi init |86,916 |86,772 |67,324 |65,524 |
| After Wi-Fi start |81,044 |80,900 |61,452 |59,380 |
| After HTTP start |68,376 |68,272 |48,784 |47,092 |
| AP ready |**67,540** |**67,436** |**47,948** |**47,092** |

The measured point-to-point net INTERNAL gain after releasing Classic controller RAM is **14,968 bytes**, not an assumed15,448-byte usable gain. The historical aggregate can rise when a new heap region is added; this confirms why minimum-ever heap cannot be used as current available memory.

Original policy remains **100,000 pre-network /49,152 operational /32,768 emergency**. The additional57,344 pre-HTTP budget/8192-byte contiguous block and4096-byte operation block fences were not relaxed. Every observed network-ready admission passed; latest pre-network119,808, AP-ready65,316, largest INTERNAL8 block36,852. No heap/init rejection, panic, canary, brownout, watchdog or unexpected reset was observed.

The earlier48,208 was historical minimum, not proof of a944-byte current shortage; its exact original rejection branch remains unrecorded. Reclaiming the unused Classic mode and BLE-only startup demonstrably restore headroom/admission, but the old trace must not be retroactively relabelled as an exact measured failure path.

## Actual timeout, BLE and Windows evidence

- Full unauthenticated initial AP window closed **180.02 seconds after ready**.
- **Nine authenticated BLE reactivations** without a USB reset during the radio-test observation windows; actual wrong HMAC and consumed-challenge replay were rejected.
- **Five score/undo cycles** produced real CCCD notifications `0,0,L,2,0,D` → `1,0,L,2,0,D` → `0,0,L,2,0,D`; normal BLE control closed the AP. Cleanup stack margins across ten observed closures: **2332–2560 bytes free**.
- **Four actual Windows connections**, not just association: Native WLAN state1, auth algorithm7 (RSNA-PSK), cipher4 (CCMP), pinned BSSID **3c:8a:1f:a1:ba:29**, DHCP **192.168.4.2** and on-link192.168.4.0/24 route. Actual RSN IE remains `0100000fac040100000fac040100000fac020000`.
- Actual webpage and pinned image identity matched; wrong admin request returned401, correct admin login/status succeeded. The following three repeat sessions all used the unchanged production `WindowsWifi` class and passed authenticated HTTP plus network restoration.
- Windows Operational events corroborate **four8001 connection successes** and security completion. Early association/security-start events still label GCMP, but the success events explicitly report AES-CCMP, agreeing with Native WLAN and actual RSN. Do not infer negotiated GCMP from the earlier event label.
- **Ethernet interface7 /192.168.1.42 source-bound TLS internet passed before, during every successful Wi-Fi session, and after**. Prior Wi-Fi disconnected/software-on state was restored; temporary profiles were not persisted. No metric/route/DNS/driver/policy change or privilege bypass.

Thus the previous practical Windows security failure no longer reproduces in this corrected configuration. Since controller mode/headroom also changed, these results do **not** uniquely prove TX cache4 was the sole cause of the earlier WPA failure.

Current boot remains initial USB metadata **UNDEFINED**, reported `bootState=undefined`, **`confirmed=false`, `healthy=true`**. It is not an OTA trial, VALID confirmation or automatic rollback demonstration. No metadata change was made to manufacture one.

## Remaining memory and shutdown findings

AP-ready current INTERNAL values through the ten windows:

```text
67540, 66064, 65956, 65968, 65864, 65768, 65660, 65576, 65432, 65316
```

After the first warm reactivation, current ready heap declined **748 bytes**, and pre-network heap declined708. Timing, deferred SDK cleanup and TCP lifetimes may contribute, but this is **not a demonstrated leak-free steady state**. Largest ready block was stable45,044 through BLE-only cycles and36,852 in the final three HTTP cycles. The post-test verification boot returned to67,536 ready bytes, but reboot recovery does not prove absence of a per-cycle leak.

Historical INTERNAL minimum reached **45,976 during WPA/HTTP**, below the49,152 *admission* number but above the32,768 emergency policy. No threshold was changed: admission uses current free heap, not this historical number. Do not claim the operational floor was never crossed transiently or that full signing/transfer-load RAM safety is proven; no upload was authorized.

**Fourteen `wifi:timeout when WiFi un-init, type=4/5` warnings persisted** in the radio-test logs despite `stop=ESP_OK`, AP_STOP acknowledgement and `deinit=ESP_OK`. Investigation used the installed ELF, not merely upstream assumptions:

- Format string is at **0x3f400bfc**, referenced by literal0x401c82fc in **`ieee80211_timer_process`** (0x401c8308).
- The warning branch acquires the Wi-Fi API lock, calls `wifi_get_init_state`, and, when state is not3, unlocks, prints this format with the timer-dispatch type argument and returns. There is no explicit timed wait on that branch. This localizes the message to a **timer dispatch arriving while Wi-Fi is not initialized**, not proof that the public deinit call timed out or that WPA failed.
- Type4/5 are retained numerically; no unsupported timer names/reason codes are invented. Their timer origin and a correct callback-drain fix remain unproven.
- Default netif destruction does attempt RX callback unregister after deinit, with an upstream comment explicitly allowing already-uninitialized Wi-Fi. However, the actual warning literal is in the timer dispatcher; changing that cleanup order alone is **not a demonstrated fix**.

No firmware/library change, warning suppression, guard reduction or second flash was attempted. Cancellation/draining of late SDK timer work and the heap drift require a separately reviewed correction and authorized update, not a speculative in-place replacement.

## Failed harnesses retained honestly

1. The first runner's literal `OTA cleanup stack free=` wait failed when an SDK warning split `free` into `fre...e`. Raw bytes show the second scoring cycle completed and2332 bytes of stack remained; no MCU reset occurred. The original failed JSON/log remain unchanged.
2. The continuation proved first WPA/DHCP/HTTP, then its **diagnostic subclass** queried connection-only Native WLAN data after disconnection (expected5023), and a second cleanup used its already-closed handle (6). This was not a WPA failure. A new native handle independently proved exact restoration; **three additional complete cycles with the production class passed**.
3. Independent validation removes only the exact known interleaved warning from a **derived parsing view**. Raw logs, warning counts, crash/rejection checks and both original failed results are retained. It does not erase a firmware failure or count mock evidence as hardware.

## Next signed wireless deployment — prepared only

Existing-source build **`agent-ota1`**,1,589,824 bytes, owner-key signature independently verified:

- Image SHA256 **`0249feffc99be29b57744b5c10298e4a1de923ef63c9d3bf212288a097867da5`**.
- Package SHA256 **`2185fe3a37999b81c89ff5f19d49a8a54ac714cb5cb9fcd16de836d0695d39a3`**.
- Exact package payload matches built image. CLI build-only preparation reran86 MSVC tests, glyph sync and48 C++ snapshots. It made no hardware activation or transfer.
- Expected next slot **`ota_1`**. Future OTA would overwrite the inactive original production fallback and update OTA selection/trial/confirmation metadata as part of the normal protocol; current recovery remains preserved. Those actions **have not happened and need fresh explicit authorization**.
- This is the currently tested source with a new build identity, **not** a shutdown-warning/heap-drift fix. No upload, staging, inactive-slot write, install request or wireless reboot occurred. No automatic update is scheduled.

## Evidence and final state

Protected session:

```text
D:\Documents\tryPython\_AgentArtifacts\pickleball_scoreboard\sessions\20261118-pi-wpa2-install\
```

Contains identity/layout/signature/write/independent-sector checks; pre/post complete recovery copies; untouched raw initial/continuation/repeat/final boot captures; failed runner results; passing repeat, native restoration and independent hardware validation; actual Windows events/RSN; SDK source/literal/disassembly evidence; and `prepared-first-wireless/agent-ota1.pksota`.

**Left `agent-wpa2`/`ota_0`, maintenance closed by normal BLE priority, initial BLE sync verified, COM11 released, Windows restored, panels still owner-confirmed unpowered/disconnected.** Final read-only recovery verification required intentional supervised resets, recorded separately from the radio-only reactivation windows. No push/merge or main change. Actual firmware OTA and its VALID health/rollback/load acceptance remain pending owner authorization.
