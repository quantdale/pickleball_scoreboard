# Authorized agent-wpa1 application-only installation — stopped at boot check

## Outcome

**Historical failure record.** The owner subsequently provided fresh electrical isolation and explicit `agent-wpa2` app-only authorization. That installation, original-threshold memory admission, timeout/BLE cycles and four real WPA/DHCP/admin-HTTP sessions pass; see [latest hardware verification](agent-wpa2-hardware-verification.md). SDK timer warnings/heap drift and firmware OTA/VALID/load/panel/rollback acceptance remain open. The current-state/safe-stop statements below apply to the earlier wpa1 session.

**The authorized application flash and independent verification succeeded. The OTA startup self-check failed, so commissioning stopped before Windows association/authentication, DHCP or HTTP testing. The WPA issue is NOT claimed fixed.** No second firmware change, heap-guard reduction, restoration, migration or provisioning was attempted after this unexpected startup condition.

The owner freshly and explicitly confirmed both panels unpowered, HUB75 disconnected while unpowered, external5V absent and ESP32 laptopUSB-only, and authorized only the signed `agent-wpa1` application. Work occurred on `feat/agent-wireless-deployment`, candidate source commit `89391a2`; no push/merge or main change.

## Pre-write safeguards and recovery

Freshly identified original **COM11**, CH340/location1-1, **ESP32-D0WD-V3 rev3.1**, MAC **3c:8a:1f:a1:ba:28**,40MHz, **4MB** flash. Read-only security checks: SecureBoot/encryption disabled, crypt count0. Stop-on-mismatch guards all passed.

Preserved corrected-current4MB image SHA256 **`6cc79fb0074c781df8e5bb9432b821c379780316bd9fd68916b2bbb19110c2d3`** was freshly checked against the actual whole-device digest and independently verified by standard esptool before writing. This transparently reuses the previously constructed/whole-device-verified complete image, not falsely described as a new full read. Two byte-identical copies were saved under the new ACL-protected session `recovery/`; original recovery copies remain untouched. They contain private NVS and must not be published. Existing original pre-OTA and baseline application recovery artifacts remain available.

The owner-key-signed package was independently verified again and its exact170-byte-header-stripped payload compared with the raw application. Image: **1,587,696 bytes**, SHA256 **`0185d1b46e16cc69f9f6e646e3f03d193b99154cafbc9fb395fdef544b535bb7`**. No signing key or provisioning credential was regenerated, printed or changed.

## Exact write and verification

- Application bytes at **0x10000**, sector erase interval **[0x10000,0x194000)**.
- Esptool upload hash passed; a separate application `verify_flash` passed.
- Independently compared **all other sectors [0x0,0x10000) and [0x194000,0x400000)** against the complete pre-write recovery image; all matched.
- **Bootloader, partition table, OTA metadata, NVS/provisioning, fallback and eFuses were not modified.** No erase-all or security override. The original fallback remains at0x200000.

## Actual boot observation

One supervised non-destructive reset/capture under the same fresh isolation:

```text
OTA build=agent-wpa1 ABI=PKSCORE-OTA-ABI1 slot=ota_0 pending=0
DISPLAY ready 64x16, chain=2, brightness=12/255
BLE advertising PickleScore; state owner=Arduino loop
OTA WiFi cleanup stop=ESP_OK event=1 deinit=ESP_OK
OTA maintenance closed; BLE available; heap=100428 min_heap=48208
OTA startup self-check failed; maintenance disabled
SCOREBOARD ready state=0,0,L,2,0,D heap=103264
```

The startup-self-check text is interleaved by SDK error output in the raw capture; the missing `OTA AP ready` assertion correctly prevented network testing. SDK Wi-Fi un-init type4/type5 warnings remain. There was one intentional reset and no observed panic, stack-canary, brownout or watchdog reset during this capture. Display-driver/BLE/scoring initialization is reported, not lit-panel or full controller acceptance.

**The AP did not pass startup and was closed.** The recorded48,208 is historical minimum INTERNAL heap, whereas `startNetworking()` checks **current** INTERNAL free heap. It does not establish a944-byte admission shortfall or identify which API/check failed. Increased TX-cache resource pressure is plausible, not proven by this number; no threshold was weakened and no speculative buffer/driver changes were flashed. Subsequent [heap diagnosis and signed, uninstalled agent-wpa2](ota-heap-admission-correction.md) documents the exact metric semantics, unnecessary Classic controller RAM and new rejection telemetry. It does not change this physical failure result or installed state.

Metadata remains initial USB state **UNDEFINED/pending0** in `ota_0`, not a pending OTA trial. Automatic boot rollback is not protection for this supervised USB installation. Prepared app/full-image recovery is retained; performing restoration would require explicit authorization, not an automatic response to the failure.

## Stage results and safe stop

| Stage | Result |
|---|---|
| Identity/security/recovery/signature |Verified |
| Authorized application-only write |Verified |
| All unrelated flash sectors |Unchanged |
| Display driver/BLE/scoring startup |Reported ready |
| OTA/AP startup |**Failed; maintenance closed** |
| Windows WPA authentication |**Not attempted after failed AP precondition** |
| DHCP/on-link IP |Not attempted |
| HTTP/page/admin login |Not attempted |
| Wireless transfer/reboot/boot confirmation |Not performed |
| Ethernet |Source-bound TLS internet passed before/after |
| Windows Wi-Fi configuration |Untouched by this boot test; adapter connection was never attempted |
| Serial monitor |Closed/released COM11 |

Session evidence:

```text
D:\Documents\tryPython\_AgentArtifacts\pickleball_scoreboard\sessions\20261117-pi-wpa1-install\
```

Contains exact identity/recovery/signature records, write plan/log, independent app/other-sector checks, frozen image/ELF/package, raw boot log and failed boot-test result. The failed run is retained, not rewritten as success. The installation script uses stop-on-mismatch safeguards and never writes outside the authorized application interval.

**Current device state:** signed USB-installed `agent-wpa1`/`ota_0`, ordinary BLE/scoring initialized, maintenance disabled by startup checks, original rollback artifacts/fallback/trust/layout intact. No further hardware operation was performed after the startup failure. Next action requires owner direction: separately authorize restoring the previous verified application, or review a focused resource/configuration correction before another app-only installation. Neither action is implicit in the completed candidate-only permission.
