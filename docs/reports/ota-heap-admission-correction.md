# OTA heap admission — agent-wpa2 software checkpoint

**Historical checkpoint.** Subsequent fresh owner authorization was used for application-only installation and actual hardware testing: measured RAM gain/admission, timeout/BLE cycles and four Windows WPA/DHCP/admin-HTTP sessions pass. SDK timer warning/heap drift remain open; no wireless firmware update was performed. See [latest hardware verification](agent-wpa2-hardware-verification.md). The software-only/not-installed statements below describe the earlier session, not current device state.

**Prepared and owner-key signed; NOT installed or physically validated.** Last verified device state remains `agent-wpa1`/`ota_0`, initial USB metadataUNDEFINED/pending0, maintenance disabled by startup checks. This session performed no ESP32 identification/reset, serial access, flash or restoration. Fresh electrical isolation and explicit candidate-specific installation authorization are required next.

## Findings and limits of attribution

1. **48,208 was NOT the admission measurement.** Arduino2.0.17 `ESP.getMinFreeHeap()` calls `heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)`; admission used current `ESP.getFreeHeap()` → `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)`. Capability-wide minima aggregate per-heap low-water values, not current available memory at the rejected branch. Adding a released heap region can also change this aggregate. The original capture does not identify the exact rejected API/check or establish a944-byte current-free shortfall.
2. `MALLOC_CAP_INTERNAL` can include32-bit-only IRAM, not exclusively byte-accessible DRAM. Total free bytes also say nothing about fragmentation. New telemetry separately measures INTERNAL|8BIT current free and largest blocks. It does **not** relabel the legacy49,152-byte INTERNAL policy as49,152 bytes of usable DRAM.
3. **The static-TX hypothesis was ruled out.** Shipped `esf_buf_setup` disassembly branches around the static pool when TX type is dynamic. Clearing the inherited8 static-TX count would not reclaim that pool; no such speculative change was made. Dynamic TX/cache behavior remains SDK-compatible with cache count4.
4. **Concrete unnecessary allocation:** pinned Arduino `btStart()` enables BTDM (Classic + BLE) because the SDK has `CONFIG_BTDM_CTRL_MODE_BTDM=1`. PickleScore uses BLE only. The installed ELF's controller region table reserves mode2/Classic memory **[0x3ffb2730,0x3ffb6388) =15,448 bytes**. The pinned `esp_bt.h` explicitly permits releasing Classic controller RAM before initialization when enabling BLE only. This is a source/ELF-proven resource defect; its contribution to the particular startup failure is not yet physically measured.

The nominal four1600-byte TX-cache frame buffers add6,400 bytes before SDK bookkeeping. They are retained, not traded away for a boot pass. Driver/task/crypto allocations are not all captured by static section sizes, and arithmetic on an old low-water number is not a substitute for a new boot trace.

## Exact correction and admission policy

- OTA profiles now check that the Bluetooth controller is idle, release **only Classic controller RAM**, initialize with `config.mode=ESP_BT_MODE_BLE`, and enable BLE before `BLEDevice::init()`. Arduino then observes an already-enabled controller rather than starting BTDM. Any precondition/API failure stops initialization, with no BTDM fallback or release of a live controller. BLE is never deinitialized/recreated for maintenance.
- This release is **RAM-only for this boot**, not an eFuse/security/persistent-mode operation. Reset/boot initializes the next application's memory afresh. The non-OTA controller path is unchanged; its generic failure log is corrected to say BLE initialization rather than incorrectly identifying every failure as queue allocation.
- `firmware/include/ota_memory.h` owns the host-tested boot ordering and admission predicate. `ble_service.cpp` remains the small actual-SDK adapter; host fakes prove ordering/failure policy, not the SDK's RF behavior or actual RAM reclaimed.
- Existing floors remain **100,000 INTERNAL bytes before networking**, **49,152 after startup/before upload and flash begin**, and **32,768 emergency closure/pending-boot failure**. No threshold is reduced and no historical minimum is used for admission.
- Add anticipatory HTTP admission: **57,344 current INTERNAL bytes** (=49,152 reserve + the unchanged8192-byte HTTP stack), plus an **8192-byte contiguous INTERNAL|8BIT block**. The original final49,152 check remains after other HTTP/route costs. Add a conservative sector-sized **4096-byte contiguous INTERNAL|8BIT fence** at ready/upload/flash-begin admission. These are additional fail-closed constraints, not claims that every SDK operation allocates exactly one sector.

### Why retain49,152?

It was introduced as a conservative software reserve in `5f748c1`, with100,000 pre-radio and32,768 emergency guards. The historical verification report explicitly lacked physical transfer-load calibration; no measured worst-case derivation was found. It leaves16,384 INTERNAL bytes above the emergency policy, **not** a proven ECDSA/Wi-Fi peak budget. Lowering it to fit48,208 would compare different metrics and remove protection without evidence. Reclaiming an unused radio mode addresses actual allocation pressure while preserving that policy; final safety/load acceptance still requires physical measurement.

### Allocation order and diagnostics

Main retains Serial → deferred boot validation → display/DMA → BLE controller/GATT → network/AP/HTTP. Unused Classic RAM is reclaimed before the BLE stack's dynamic allocations and before Wi-Fi, avoiding late release/reallocation of a live radio. Wi-Fi remains4 static/8 dynamic RX,8 dynamic TX, cache4, aggregation off; HTTP retains8KB stack/two sockets. The4KB cleanup stack is static and unchanged. Upload/readback buffers remain1KB stack buffers; crypto/driver/HTTP allocations remain checked by their existing failure paths.

Bounded stage telemetry reports:

```text
OTA HEAP stage=... free=... min=... free_internal8=... largest8=... largest_internal8=...
OTA HEAP rejected stage=... free=... min=... free_internal8=... largest_internal8=... floor=... block=...
OTA INIT rejected stage=... error=...
```

Stages cover before/after Classic release, enabled controller/GATT, netif/Wi-Fi/HTTP startup, session admission, signature verification (including failure), flash begin/readback, HTTP/Wi-Fi/final closure, and boot confirmation. Init API checks have separate stage/error labels rather than a short-circuit chain with an unknown return path. Fixed stack formatting buffers avoid Arduino's long-`Print::printf` temporary heap allocation. These are stage samples, not hot-loop tracing; existing emergency checks remain unchanged.

## Before/after evidence

| Measurement | agent-wpa1 physical capture | agent-wpa2 |
|---|---|---|
| Post-closure current INTERNAL |100,428 |Not measured |
| Scoreboard-ready current INTERNAL |103,264 |Not measured |
| Logged minimum INTERNAL |48,208 |Not measured |
| Current INTERNAL at rejected branch |Not captured |New exact rejection telemetry; not run |
| Current INTERNAL\|8BIT / largest block |Not captured |New stage telemetry; not run |
| Unused Classic region retained |15,448-byte region established from installed ELF/SDK |Boot release implemented; expected gross region15,448 **before allocator overhead**, not a measured gain |
| AP / Windows security / DHCP / HTTP |AP startup failed; network test not attempted |Not attempted |

No progressive-leak/fragmentation or startup-order attribution can honestly be established from the previous capture. New after measurements remain deliberately blank until authorized hardware work.

## Verification completed

- **86 MSVC Unity tests**: original78 plus8 memory/controller tests. Includes min-versus-current admission, exact boundaries, fragmented/incorrect-capability largest blocks, HTTP stack budget, no live-controller release, failure ordering and no BTDM fallback. The legacy-default-startup **host model** failed3 new tests before the policy correction; that is not an MCU heap reproduction.
- **29 CLI tests**, including mocked failure/cleanup/reconnect/security cases.
- **15 real OpenSSL/package tests**, with disposable TEST ONLY keys outside Git. An initial incorrect `unittest discover` invocation failed harness setup and ran0 tests; it is preserved separately. The supported script entry point subsequently passed all15; no product-code repair was needed.
- **42 Android JUnit tests**, forced `--rerun-tasks` after the first invocation reported up-to-date. Glyph/layout synchronization and **48 full C++/shared snapshots** passed; Kotlin consumes the same fixtures. No new emulator/device/UI run is claimed.
- Real CLI build/sign/verify integration for `agent-wpa2`; `ota`, `ota_b` (`agent-wpa2-b`) and `esp32dev` builds pass. Audited actual partition tables, exact preserved baseline bootloader, rollback ELF instructions and strong deferred-confirmation symbol pass. This is not physical rollback execution.
- C++ LSP probes timed out: **not confirmed clean**. Actual SDK compilation and host tests passed. Markdown LSP checks completed; a dictionary warning on the verified MAC's `ba` octet was marked false-positive.
- Build-only preflight verified Ethernet-source-bound TLS internet and restored the Wi-Fi snapshot. No AP activation/connection or wireless upload was attempted.

Scoring handlers, BLE UUIDs/commands/HMAC protocol, Android sources, renderer/shared pixels, pins/brightness, trust record, signature checks, A/B layout and rollback behavior were not changed.

## Signed candidate and evidence

Session:

```text
D:\Documents\tryPython\_AgentArtifacts\pickleball_scoreboard\sessions\20261117-pi-ota-heap-admission\
```

Use **`candidate-final/`**, not the earlier preliminary `candidate-build/`:

- Build **`agent-wpa2`**, image **1,589,824 bytes**; slot headroom441,792.
- Image SHA256 **`db32e3d046acdfb775797d0d504cb0074445d6bb6ead7171805feb91f0555dea`**.
- Owner-signed package SHA256 **`726af4d7f8aa6de687076af63c27a8454ae6f8e8117177a354801d2397a7c5b3`**.
- Independent owner-public-key verification, exact170-byte-header-stripped payload comparison and audited-image equality pass. Existing owner credentials/signing key were not regenerated, printed or changed.
- If separately authorized later: app offset0x10000, proposed sector interval **[0x10000,0x195000)**. This is **not an executed or implicitly authorized write**. No migration/bootloader/metadata/NVS/fallback/eFuse operation is included.
- SDK sources/disassembly/region analysis, red/green host results, build/audit/signature/CLI/Android logs and candidate manifest remain external. Protected recovery from the prior installation remains untouched; it must be freshly checked against the actual target before another write.

## Remaining physical gates

1. Obtain fresh owner confirmation: both panels unpowered, HUB75 disconnected while unpowered, external5V absent, ESP32 laptopUSB-only; explicitly authorize this signed `agent-wpa2` **application only**. Reidentify board/security and freshly verify complete recovery/current state before writing. Stop on any mismatch/unexpected condition; no automatic restoration or USB fallback.
2. After an authorized installation, capture actual Classic-release delta, current/byte/contiguous heap at each stage and actual `OTA AP ready`. If admission/API checks fail, retain the exact rejection trace and stop rather than reducing a floor.
3. Verify full timeout and at least three authenticated BLE activation → AP closure → CCCD/rally/undo cycles. Compare **settled same-phase current heap and largest INTERNAL|8BIT blocks**, allowing initial SDK/lwIP warm-up but not a continuing downward trend. Retain cleanup-task stack margins, timing and strict raw panic/watchdog/reset checks. Old successful agent-v2-a cycles do not validate this controller-mode change.
4. Only with AP startup proven, establish actual Windows WPA2/CCMP security-connected state, DHCP/on-link route, pinned authenticated HTTP identity, and restored Wi-Fi/Ethernet internet. Then separately perform the authorized signed wireless updates/confirmed-health and load gates. Persistent SDK un-init warnings remain unresolved until measured; do not claim WPA, OTA, lit-panel or rollback acceptance from software results.

Work remains on `feat/agent-wireless-deployment`; no push/merge or main update.
