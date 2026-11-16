# Dual-P10 production flash verification

## Result

**Production firmware uploaded and USB-only boot verified on2026-11-16. Physical display and real phone/BLE E2E remain untested.** No panel power or HUB75 reconnection was performed by the agent. The accepted minor border issue was not reopened.

Firmware source checkpoint: `3bf53dd44942be7fae465e6fe2f51f7b7cb148dd` on `feat/dual-p10-scoreboard`. Documentation/status updates after this checkpoint do not change the flashed firmware. `main` remains `a08a30c92ecd8af6b6d1efaefda5b7aab9b106aa`; feature commits are local/unpushed.

## Safety and target identification

The owner freshly confirmed panel supply disconnected/discharged, HUB75 disconnected while unpowered, ESP32 powered by USB alone, and no external5V feed. Only after that confirmation were serial-resetting identification and upload performed.

- Port: **COM11**, CH340 USB VID:PID1A86:7523, location1-1.
- Actual chip: **ESP32-D0WD-V3 revision3.1**,40MHz crystal,4MB flash.
- MAC: **`3c:8a:1f:a1:ba:28`**, matching the intended previously identified board.
- Read back290,720 bytes of the previous application at0x10000 before writing. SHA256 exactly matched the preserved accepted boundary image: `f9f194a50dd0eb989f914af49c57222f423764c94ae42741ca52c55d628e06ea`. This is an application backup, not a complete4MB flash backup.

## Upload and independent verification

Executed at115200 baud:

```bash
PYTHONIOENCODING=utf-8 firmware/.venv/Scripts/pio.exe run -d firmware -e esp32dev -t upload --upload-port COM11
```

PlatformIO succeeded; bootloader, partition table, boot_app0 and application each reported **Hash of data verified**. An independent esptool `verify_flash` then compared the full1,156,448-byte application at0x10000 against the frozen deliverable and returned **verify OK (digest matched)**.

Application SHA256: `14e77d5d48aee05652276a2757277439e89dff6d92614b76a0882294638228ae`. No firmware or upload failure occurred; no production code change or speculative hardware adjustment was needed.

## Actual serial boot observation

A controlled EN reset was captured at115200 baud beginning `2026-11-16T10:09:23+08:00`, followed by approximately120 seconds of USB-only runtime:

```text
[1.030s] DISPLAY ready 64x16, chain=2, brightness=12/255
[1.733s] BLE advertising PickleScore; state owner=Arduino loop
[1.738s] SCOREBOARD ready state=0,0,L,2,0,D heap=135592
```

- One intended reset and exactly one ROM reset message in the capture.
- DMA display initialization returned success; authoritative boot state is left0/right0, left serving, server2, not ended, DOUBLES.
- Firmware reported BLE service/advertising initialization. Over-the-air advertising, GATT subscription and phone control have **not** been independently tested.
- Free heap after BLE initialization: **135,592 bytes**. No repeated heap telemetry was emitted while idle; this does not establish long-running/load heap stability.
- No unexpected reset, panic, assertion, watchdog, brownout or serial error observed during the capture.
- Monitor closed and **COM11 released** afterward.

## APK, rollback and evidence

Latest evidence/deliverables root:

```text
D:\Documents\tryPython\_AgentArtifacts\pickleball_scoreboard\sessions\20261116-pi-scoreboard-upload\
```

- Current APK: `deliverables/android/pickle-scoreboard-debug.apk`, SHA256 `79854fe88ab4a20e18f8a5ceaef7d2d5443d13f0835f02836d82673fe431715f`. Signature verification and actual API35 emulator installation/launch passed in the preceding commissioning checkpoint. No real phone is claimed tested.
- Rollback: `deliverables/rollback/boundary/` contains the accepted application and companion boot images. `pre-flash-boundary-application.bin` is the matching actual board readback. Original artifacts and source/Git bundles remain preserved in the earlier commissioning session.
- Logs: `chip-identification.log`, `flash-identification.log`, `pre-flash-read.log`, `upload-production.log`, `verify-production-flash.log`, `serial-raw.log`, `serial-timestamped.log`, `serial-verification.json` and `artifact-manifest.json`.

## Next physical action

1. **Unplug ESP32 USB first.** Keep the panel supply disconnected/discharged. Reconnect accepted HUB75 wiring only while everything is unpowered: ESP32→Panel1 J1, Panel1 J2→Panel2 J1; Panel2 J2 empty.
2. Retain accepted common ground and regulated5V panel supply; upstream12V feeds the converter only, never panels/ESP32. Recheck polarity before energizing. ESP32 remains USB-powered.
3. Install the current APK on the owner's phone, connect to `PickleScore`, and follow the [physical E2E checklist](../commissioning-dual-p10.md#physical-e2e-acceptance--all-pending-owner-observation). Initially expect0–0 with two left arrows. Rally Left→1–0/left/server2; Rally Right→1–0/right/server2; Rally Right→1–1/right/server2; Undo→1–0/right/server2; Switch Courts→0–1/left/server2.
4. Verify end/freeze/reset/singles/reconnect and sustained normal operation. The preserved nonstandard doubles full side-out to server2 still requires owner gameplay acceptance.

No physical acceptance checkbox is closed by upload success or serial diagnostics. Any later reflash/rollback requires a new isolation confirmation after the wiring has changed.
