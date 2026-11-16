# Local browser OTA: software feasibility and verification

**Historical software-only checkpoint.** Initial approved USB migration/provisioning and actual startup have since been performed; see [installation evidence](browser-softap-ota-installation.md), including observed heap and the unresolved Wi-Fi shutdown warning. The no-hardware statements below describe the earlier development session, not current deployment status.

## Result and scope

**Implementation/builds/automated software checks pass. Initial hardware migration and actual OTA operation are NOT performed or accepted.** The owner's working installed scoreboard is untouched. Browser screenshots use the actual embedded HTML/JS with a **local mock backend**, not the ESP32. No claim of RF, power, heap-load, bootloader-execution or two-wireless-update success is made.

Branch: `feat/browser-softap-ota`, forked from preserved `5ae28e2` (`3bf53dd` production source). Initial working tree was clean. `git fetch origin` completed; fetched `origin/main` remained `3ae7ad2`, with no new remote commits beyond the known documentation base. Existing feature commits are verified present. No main/old-feature ref changes, merge, push, flash, serial reset, partition write or eFuse operation occurred during OTA development.

The owner's report that the existing physical display boots and both panels work is accepted **baseline display evidence only**, not OTA/BLE coexistence acceptance.

## Inspected toolchain and feasibility

- ESP32-D0WD-V3/4MB is the previously verified physical baseline; no fresh chip interrogation was done without a new isolation confirmation. COM11 currently enumerates as CH340 (metadata only).
- PlatformIO6.1.19, pinned espressif32 platform6.13.0, Arduino-ESP322.0.17, **ESP-IDF4.4.7**, Xtensa GCC8.4.0. Versions were read from actual installed headers/build output. SDK Mbed TLS is2.28.7.
- HUB75 DMA3.0.15/GFX1.12.6 and all pins/scan/clock/routing/brightness are unchanged. Scoring/display implementations, shared pixels and Android files have no diff from5ae28e2.
- Actual baseline binary is **1,156,448 bytes**, SHA256 `14e77d5d48aee05652276a2757277439e89dff6d92614b76a0882294638228ae`. Existing binary partition table has two0x140000-byte app slots, NVS/otadata, old SPIFFS and coredump. The compiled OTA image does **not** fit those old slots.
- New explicit CSV provides two0x1f0000-byte slots (**2,031,616 each**), NVS20KB, OTA metadata8KB and coredump64KB, with no filesystem. Parsed generated binary tables have no overlap and end exactly at0x400000. Slot addresses0x10000/0x200000 and subtypesota_0/ota_1 are verified. Runtime code refuses any other layout or flash size.

### Final real builds

| Profile | Build ID | Linked flash bytes | Actual image bytes | Slot headroom | Static data+BSS |
|---|---|---:|---:|---:|---:|
| `ota` |`ota-v1-a`|1,577,329|1,583,904|447,712|58,900|
| `ota_b` |`ota-v1-b`|1,577,329|1,583,904|447,712|58,900|
| `esp32dev` (non-OTA regression) |baseline protocol/layout|1,149,869|1,156,448|old layout retained|39,292|

Both OTA variants use approximately110,690 bytes of instruction RAM. Static data/BSS is NOT total runtime consumption: DMA, BLE, HTTP task/socket, ECDSA and Wi-Fi buffers need additional heap/stack. Previous baseline free heap135,592 bytes after BLE is historical hardware evidence, not a new OTA measurement. No minimum OTA runtime heap is claimed.

Resource controls:4 static/8 dynamic RX Wi-Fi buffers,8 dynamic TX, aggregation disabled, one AP station, two HTTP sockets,8KB HTTP stack,1KB transfer/readback buffers, minimum100KB free heap before radio startup and49,152 bytes after startup/before transfer. Critical low heap closes maintenance; pending startup failure rolls back. HTTP teardown runs on a reserved cleanup task instead of blocking the scoring loop. BLE stack is retained rather than repeatedly deinitialized; lwIP cannot be fully deinitialized in this SDK. **Actual coexistence/fragmentation/stack/watchdog load testing remains required before sealing.**

## Real rollback capability, not inferred API support

1. Inspected actual DIO SDK header: `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=1`; hardware Secure Boot and native signed-app mode are disabled. No configuration/eFuse security was silently relaxed.
2. Inspected actual shipped `bootloader_dio_40m.elf` disassembly: it reads OTA-state field at offset24, converts PENDING_VERIFY(1) to ABORTED(4), selects valid metadata, then converts NEW(0) to PENDING_VERIFY(1), writing metadata through `write_otadata`.
3. ELF SHA256: `a22955bf4c3bed6e0ad99e6832694e999cf1e744bfdf969140ba7825cc77a5a3`. Generated OTA bootloader binary **exactly matches preserved installed-baseline bootloader** SHA256 `3d234a7471f67b013686dabd4dee7c1fa915c9928463616a94bc9297acf1abf8`. No replacement is needed or authorized automatically.
4. Arduino's actual early default confirmation code was inspected. Both OTA ELF files link a **strong** `verifyRollbackLater` symbol, overriding the weak default. Pending images receive a30-second startup watchdog and wait for initialization plus5 seconds/100 healthy loop iterations before `esp_ota_mark_app_valid_cancel_rollback`.
5. First USB migration is **not** an automatically protected pending trial. It needs supervised USB validation/provisioning. A seeded baseline at the second new-slot address provides a bootable fallback, not a permanent rescue slot; the first wireless update overwrites it while preserving the current OTA app. Ordinary OTA rollback after that first confirmed update retains an OTA-capable previous slot.

The compiled bootloader/app capability is genuinely present, but rollback has **not been physically triggered** here. It cannot rescue both damaged slots, failed first partition migration, behavior after confirmation, lost keys or physical flash tampering. See [migration/recovery guide](../ota-maintenance.md).

## Executed test evidence

| Area | Executed result | Boundary |
|---|---|---|
| Scoring17/parser22/display6/protocol6 |**51/51 Unity PASS**|Existing hardware-free production tests |
| New OTA core |**18/18 Unity PASS**|Same policy, request/auth predicates, header/stream transaction as production; fault-injecting flash sink |
| Total firmware host suites |**69/69 PASS**|MSVC/Unity; unrelated MinGW issue unchanged |
| AP/session/transfer deadlines, wraparound, five-failure login cap |PASS|Actual pure policy; not Wi-Fi radio |
| Exact Host/Origin/URI, missing/old auth, one-use nonce consumption and replay |PASS|Actual shared predicates/session state; not live SDK HTTP parser |
| Wrong target/size/overflow/padding/invalid ESP32 header, truncation/oversize/interruption |PASS|Actual shared transaction with fake flash, no boot selection on rejection |
| Begin/write/finalize/readback/select failure injection |PASS|Simulated allocation/flash/validation faults, running fake image remains unchanged |
| Duplicate/concurrent transaction admission and explicit install gating |PASS|Single HTTP worker plus tested policy/transaction guards; no arbitrary flash targets |
| Repeated alternating updates |Eight simulated confirmed reboots PASS|Fake sink alternates inactive slots; NOT eight real OTA updates |
| OpenSSL signing/package/provisioning tools |**15/15 PASS**, final actual A/B images|Real disposable P-256 signing/verification, wrong key/curve/tamper/checksum/trailing/length cases; no owner credentials generated |
| Exact generated partition binary and linked override |PASS|Actual compiled files, not comments |
| Embedded JavaScript syntax |PASS (`node --check`)|Actual embedded source |
| Embedded browser flow |PASS in actual Chromium at desktop and390×844|Actual HTML/JS, **local mock backend only**; failed login/invalid package, raw upload, stage-without-install, cancel/accept confirmation, no horizontal overflow/no JS errors |
| Glyph/layout drift and C++/Android fixture data |PASS; **48 complete snapshots**|Actual C++ exporter and unchanged shared fixtures |
| Android regression |**42/42 JUnit PASS**, fresh rerun; APK and lint tasks PASS|No Android source edits; existing lint baseline/AGP compileSdk35 warnings remain |
| Production ESP32 builds |**esp32dev/ota/ota_b PASS**|No upload target invoked |

LSP probes were performed; some C++ paths timed out, so the successful actual Xtensa compilations/tests are the software evidence, not an empty diagnostic cache. Remaining heuristic numeric-conversion warning applies to regex-extracted digit-only build-tool output, not untrusted browser input. Existing firmware-host scripts remain intact.

### What is still not tested on the device

- Wi-Fi AP/DHCP/browser reachability and OS “no internet” behavior on Windows/Android/iPhone.
- Actual Mbed TLS verification under MCU heap pressure and SDK flash/end/readback behavior during a real transfer.
- Hardware interruption recovery, repeated real slot alternation, pending-boot watchdog/rollback execution.
- BLE/AP/DMA coexistence timing, normal controller races, LED stability during flash writes, reclaimed heap, fragmentation, stack watermark and long-running watchdog stability.
- Two consecutive owner-observed wireless updates and subsequent real scoring/renderer/BLE regression.

These remain explicit acceptance checks, not hidden failures waived by compilation. No artificial failure app is pre-authorized or mixed into production releases.

## Artifacts and reproducibility

Session root:

```text
D:\Documents\tryPython\_AgentArtifacts\pickleball_scoreboard\sessions\20261116-pi-softap-ota\
```

**Final public firmware/recovery artifacts are under `release/`** (`deliverables/` is an earlier software checkpoint):

- `release/ota/{firmware.bin,firmware.elf,partitions.bin,bootloader.bin}`
- `release/ota_b/{firmware.bin,firmware.elf,partitions.bin,bootloader.bin}`
- `release/initial-otadata.bin`
- `release/baseline-usb-recovery/` (the actual commissioned original, not the newly rebuilt baseline)
- `release/build-report.json` records exact sizes, partition tables, public file hashes and false hardware-test flags.
- Accepted boundary-test application/companion recovery files are also preserved in the session and original upload evidence. It is a diagnostic fallback, not the OTA baseline.

Evidence: `build-release.log`, `host-tests.log`, `signing-tests-release.log`, `tool-tests-release/result.json`, `android-tests-fresh.log`, copied Android test XML, `browser-ui-result.json`, browser screenshots and inspected bootloader/sdkconfig evidence. `baseline.bundle` preserves the previous working branch; the final feature source/Git bundle/manifest accompany the local commit.

Only disposable **TEST ONLY** signing/provisioning files/packages exist under `tool-tests*`; they are outside Git and outside public `release/`. No production password/private trust key has been chosen. For real installation, the owner generates private files using `scripts/ota_tool.py`, provisions that trust, then signs A/B with that key. The raw firmware is ready, but a universally trusted/pre-signed default package would violate the no-default-credentials/trust requirement.

## Hardware boundary and remaining authorization

Stop before opening/resetting the ESP32. The next hardware phase requires BOTH:

1. Fresh confirmation: panel supply disconnected/discharged; HUB75 disconnected while unpowered; ESP32 powered by USB alone with no external5V.
2. Explicit authorization of the documented **partition-table migration, OTA-metadata initialization, initial application write and fallback seed**, after full-flash backup and identity/bootloader/security verification.

The earlier production-flash isolation confirmation does not authorize this new migration or imply the installed working panel wiring is still isolated. No automatic erase/bootloader replacement is proposed. The full step-by-step provision/sign/browser/two-update procedure and risks are in [docs/ota-maintenance.md](../ota-maintenance.md).
