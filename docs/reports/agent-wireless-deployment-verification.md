# Agent wireless deployment: implementation and actual commissioning

## Verdict

**Historical commissioning checkpoint.** Subsequent authorized `agent-wpa2` app-only installation and actual heap/AP/BLE/Windows WPA/DHCP/admin-HTTP tests pass; see [current hardware verification](agent-wpa2-hardware-verification.md). Timer-shutdown warnings and heap drift remain unresolved, and actual firmware OTA/VALID/load/rollback/panel acceptance is still pending. The WPA-blocked/current-agent-v2-a statements below describe this earlier session, not the latest installed state.

**Not end-to-end commissioned.** Software CLI/build/signing and real authenticated BLE maintenance activation are implemented. The Windows Killer AX1650i adapter repeatedly stalls in WPA authentication to the actual ESP32, before DHCP or HTTP. Independent native `netsh` reproduces this with the same protected PSK. **Zero wireless firmware uploads/installations succeeded or were sent to HTTP in this session.** Do not describe the two USB application installations as OTA success.

Branch: `feat/agent-wireless-deployment`, based on `31f49e4`. Main is not edited/merged/pushed. See [CLI usage/security](../agent-ota-deployment.md) and Spec04 §8. This remains an active OpenSpec change with physical gates unchecked, not an archived/completed rollout.

## Implemented

- `tools/ota_deploy.py`: explicit owner-authorized lifecycle, registry/target/public-key pins, tests/build/sign, exact existing HTTP protocol, separate stage/install/reboot/health results, bounded retries and no USB-flash fallback. Boot health checks require expected ID/opposite slot/exact image hash/VALID startup; legacy images require safely available serial confirmation.
- `tools/ota_windows.py`: Native WLAN discovery/radio snapshots/in-memory temporary manual WPA2/AES profile, SSID/BSSID/on-link IP route checks, explicit WLAN source-bound HTTP, Ethernet-route plus certificate-verifying source-bound TLS internet probes, restoration and adapter-wide mutex. No metric/DNS/default-route/driver/policy changes. Per-device exclusive lock protects deployment/signing artifacts. WSL delegates to Windows Python; actual WSL commissioning is unverified.
- Dedicated BLE maintenance read/write characteristic `...26aa`, expiring connection/target-bound random challenge and HMAC proof with explicit match-loss flag, single-use consumption, connection invalidation and five-failure cooldown. Existing scoring service/commands/CCCD/queue/scoring/rendering remain unchanged.
- Loop-owned AP reactivation after authenticated central disconnect, credential reload and cleanup completion. Info fields expose exact running application image SHA256, startup/boot state and confirmation. Signed package/partition/rollback protections unchanged.
- Explicit validated `PICKLESCORE_BUILD_ID` build integration; signing/build audit tests read actual compiled IDs rather than hard-coded legacy A/B labels.

## Hardware identity, permissions and written ranges

Electrical setup retained from the owner's explicit preceding confirmation: panels unpowered, HUB75 disconnected while unpowered, laptopUSB-only and no external5V. COM presence alone was not treated as isolation proof. Current task explicitly allowed PhaseB USB **application-only** installation after identity/current-backup checks. No provisioning/migration was repeated.

Actual identity rechecked: **COM11**, CH340/location1-1, ESP32-D0WD-V3 rev3.1, base MAC **3c:8a:1f:a1:ba:28**,4MB/40MHz. Read-only SecureBoot/encryption/crypt-count checks remain unchanged/disabled. Real AP BSSID **3c:8a:1f:a1:ba:29**, BLE address **3c:8a:1f:a1:ba:2a**; these are not interchangeable.

1. First explicit USB application install: `agent-v1-a`,1,587,584 bytes at0x10000; sector erase ends0x194000 exclusive. SHA256 `62a6d87e29e86ef8e4c124dd5946a999cb9e626c0b279e66fd3b66847a6f9724`. Required full-current-flash match against the verified4MB `e3b0930d...` backup before writing. Independent app verification and every other flash sector matched afterward.
2. Raw idle-timeout diagnostics exposed a real cleanup task **stack-canary panic**. The early BLE test summary passed its protocol assertions after an unexpected reboot; it is explicitly NOT stability evidence. The raw log, failed attempts and crash are retained. Actual SDK `StackType_t` is uint8_t, making the inherited1024-element cleanup stack only1KB. This was fixed to an explicit4096-byte static stack with compile-time size assertion, measured free stack and low-margin reactivation refusal.
3. A fresh complete4MB backup was required before the correction. The1024-byte serial read failed at0x281800; no partial data was accepted as a complete backup and no write followed that failed read. A512-byte/one-inflight read succeeded across64 independently verified64KB chunks, full-device MD5 before/after, exact snapshot digest and separate recovery copy. Standard esptool full-image verify passed again before writing.
4. Stack-fix USB application install: **`agent-v2-a`**,1,587,696 bytes at0x10000; same erase end0x194000. SHA256 **`b190a900fdd908c73d846a408490c5a117e584d0e869b5bcdbd05bd0de5459c9`**. Upload hash and separate esptool app verify passed; all sectors0x0..0x10000 and0x194000..0x400000 exactly matched the fresh full backup afterward.

**No bootloader, partition table, OTA metadata, provisioning record or fallback writes. No erase-all, eFuse/security changes, force override or silent USB fallback.** Coredump diagnostics from the earlier crash are preserved, not erased. Metadata remains the initialized USB state UNDEFINED/pending0 in `ota_0`; this is not a confirmed OTA trial. Current non-OTA production fallback in `ota_1` remains unchanged.

## Fresh recovery backup

Protected session root:

```text
D:\Documents\tryPython\_AgentArtifacts\pickleball_scoreboard\sessions\20261117-pi-agent-wireless\
```

`before-stack-fix/current-full-flash.bin` and `before-stack-fix/recovery/current-full-flash.bin`: each4,194,304 bytes, SHA256 **`ee4547e8b0eef9e4abcd7cf2f770b6a2abaa148bf3eeec7248f306fffe4ec0dc`**, MD5 `dc93ccedf8762de7fe946b3cb19e4f32`. This is the exact **pre-correction** installed state, including the known1KB cleanup-stack application/coredump, retained for audit; do not treat it as a preferred stable rescue firmware. It contains private NVS and remains ACL-restricted. The original pre-OTA4MB backup/recovery `707031c1...` and prior commissioned OTA-state4MB backup `e3b0930d...` remain untouched. Refer to their exact `USB_RECOVERY.md` files; a destructive complete restoration requires separate explicit approval/fresh isolation and can discard credentials. No recovery restoration was exercised.

A complete corrected-current recovery image is additionally preserved at `current-corrected-recovery/current-full-flash.bin` with a separate identical copy, SHA256 **`6cc79fb0074c781df8e5bb9432b821c379780316bd9fd68916b2bbb19110c2d3`**. This was transparently derived from the verified complete read plus the exact app write/erased-sector padding, NOT represented as another physical full read. An independent whole-device digest and standard esptool verification of all4MB both matched; it retains actual NVS/metadata/fallback/coredump bytes. Use only with separately authorized full restoration.

Frozen `stack-fix-install/agent-v2-a.bin/.elf/.pksota` preserve the corrected installed image and actual owner-key signature. `v2-deliverables/` preserves both audited corrected build variants and baseline recovery; signing private material is never included. New owner keys/PSKs were NOT generated; the existing protected provisioning/signing trust was reused. Default external registry is `%USERPROFILE%\.picklescore\devices.json` with owner/SYSTEM-only directory ACL.

## Software tests

| Check | Actual result |
|---|---|
| CLI protocol/lifecycle/network mocks |29 tests pass; includes wrong identity, bad credentials, Ethernet loss, interrupted upload, signature/size/slot rejection, uncertain install not replayed, wrong reboot version/hash/health, restoration failure and pre-proof-only BLE retries |
| Firmware Unity/MSVC |77 pass (69 existing +8 activation policy tests) |
| Real OpenSSL signing/package tools |15 pass with disposable TEST ONLY keys; production trust separately checked |
| Shared JSON/C++ rendering snapshots |48 complete64×16 frames agree; glyph/layout checks pass |
| Android JUnit |42 fresh tests,0 failures/errors; no Android code changes |
| ESP32 builds |ota, ota_b and non-OTA esp32dev pass; corrected OTA variants both built/audited |
| Corrected app size |1,587,696 bytes;443,920-byte slot headroom; data+BSS62,164; instruction RAM110,730 |
| Rollback audit |Same inspected SDK ELF/rollback transitions and strong delayed-confirmation linkage; no physical rollback execution |
| CLI build/sign integration |Actual `build --device court1 --build-id agent-v2-cli1` passed firmware/glyph/snapshot tests, PlatformIO compilation, owner-key signing and independent package verification without activation |
| Python validation |Ruff clean and actual unittest imports/execution pass. Active LSP probes repeatedly timed out; no clean-LSP claim is made |

No new emulator/illuminated-panel acceptance is claimed. Mocked deployment success does not prove ESP32 transfer/reboot behavior.

## Actual BLE/scoring/lifecycle evidence

With corrected `agent-v2-a`, one supervised reset then the complete180-second startup AP expiry passed without panic or unexpected reset. Three additional authenticated BLE activation→AP startup→scoring connection/AP closure→CCCD sync→rally→undo cycles passed, **without additional resets or flashing between cycles**. Wrong HMAC is rejected and consumes the challenge. Earlier real checks also rejected old-connection/replayed successful proofs; their stability summary is superseded by strict corrected-build raw-log analysis.

- Initial AP-start free heap55,624; scoreboard-ready55,568 bytes.
- Reopened AP free heap54,192→54,080→53,988 bytes.
- Minimum observed heap after three cycles49,228 bytes. This is not transfer-load or long-duration stability proof.
- Four cleanup stack margins: **2576,2720,2720,2492 bytes**, no stack-canary event or unexpected reset.
- BLE state packets per scoring cycle: `0,0,L,2,0,D`→`1,0,L,2,0,D`→`0,0,L,2,0,D`.

The lifecycle runner initially rejected its final margin count because Wi-Fi SDK output interleaved inside one stack-margin print. Original failed runner output remains. A separate deterministic raw-log validator checks the numeric margin, exact reset count, absence of panic/canary/backtrace and all three activation/scoring cycles; it records the validated result separately, not by deleting the original failure.

**SDK shutdown warning remains unresolved:** `wifi:timeout when WiFi un-init, type=4`, sometimes type5, occurs even with stop=ESP_OK, AP_STOP acknowledgement and deinit=ESP_OK. Repeated BLE/AP/scoring cycles worked with the stack correction, but the warning is not suppressed or called fixed, and the WPA/transfer problem is not attributed to it without proof.

## WPA blocker and network evidence

Repeated actual attempts: legacy installed OTA-A, agent-v1-a, corrected agent-v2-a; Native WLAN saved user profile, native in-memory temporary profile, a no-11n profile experiment and independently invoked Windows `netsh`. Wrong-target/BSSID preflight was fixed before HTTP; transient authentication states are not counted as connected. Retries no longer interrupt a live WPA handshake. Temporary profile encryption prevented plaintext round-trip inspection; an encrypted key mismatch was **not** used as evidence of a wrong password. A raw-PMK experiment did not improve connection; it was removed. No authentication/signature policy was weakened.

Windows associates with expected SSID/BSSID but remains authenticating, without on-link192.168.4.x routing. Windows WLAN events and serial boot/activation logs are preserved. No HTTP maintenance key is sent before WLAN/BSSID/routing checks. Independent `netsh` test reproduced WPA/DHCP timeout and removed its own protected temporary XML/profile afterward. BLE occasionally times out in WinRT service discovery; bounded fresh-connection retry is limited to stages before proof transmission, never replaying an uncertain proof or evicting a scoring controller.

Ethernet index7/source192.168.1.42 remained the route to internet and passed source-bound certificate-verifying TLS probes before/after failed WLAN attempts. Final task-entry restoration returned Wi-Fi to disconnected/software-off/hardware-on, with Ethernet TLS internet verified again. WLAN profile/radio cleanup was checked per operation. No routes, metrics, DNS, drivers, firewall/security policy or other saved Wi-Fi credentials were modified. Current hardware limitation must be isolated with an additional Wi-Fi client/adapter or targeted safe packet/driver evidence before claiming a working wireless deployer; it is not permission to bypass WPA, expose an open AP, re-provision or migrate flash.

## Outstanding acceptance

- Real HTTP login/upload/install/reboot and exact running-image/confirmed health verification.
- Two distinguishable signed BLE-triggered wireless updates; CLI still returns failure while WPA is blocked.
- Physical invalid-signature/interrupted-upload behavior, transfer-load heap/stack/watchdog and sustained normal operation.
- Controlled automatic rollback using a separately reviewed, explicitly authorized failure fixture; no destructive rollback test was improvised.
- Lit panels/Android physical E2E remain pending while panels/HUB75 are disconnected. Scoring protocol/rally/undo over real BLE was exercised, not light output.
- Resolve/accept the shutdown warning with appropriate operational evidence; do not seal on serial startup alone.

**Safe final state:** corrected USB-installed `agent-v2-a`/`ota_0`, original fallback/trust/layout intact, serial released. This session delivers executable software and actual BLE/stack evidence, but deliberately does not claim the master's final wireless success criterion.
