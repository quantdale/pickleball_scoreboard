# Pickleball scoreboard

ESP32-owned scoring with an Android BLE remote. PickleScore 2.0 shows
**LEFT SCORE | SERVING ARROWS | RIGHT SCORE** on two 32×16 P10 panels (64×16,
default brightness **12/255**). The landscape controller sets each team's score with `+`/`−`
and the serving arrow by hand. See the [operator guide](docs/landscape-operator-guide.md)
and [2.0 release candidates](docs/reports/landscape-2.0.md). Android 2.1.0 and
`court-brightness-001` add operator brightness from the Board menu; see the
[brightness report](docs/reports/brightness-2.1.md). Android 2.2.0 and
`court-color-001`/`-002` add one LED color for every lit pixel, remove the
Undo key and restore OTA heap headroom; see the [2.2 report](docs/reports/release-2.2.md).

Court 1 now runs `court-landscape-001` on `ota_0`, installed over USB after the
wireless route was [refused for heap](docs/reports/court1-landscape-ota-blocker.md).
See the [USB install report](docs/reports/court1-landscape-usb-install.md). The portrait
release is preserved at tag `portrait-1.3.1-court-undo-003`.

- [Agent deployment CLI and authenticated BLE activation](docs/agent-ota-deployment.md) — routine installed OTA leaves approved power and both panels connected; no further write is authorized. [Latest endurance evidence](docs/reports/ota-lifecycle-endurance.md).
- [Local SoftAP/browser OTA: provisioning, security, migration and two-update guide](docs/ota-maintenance.md)
- [Actual OTA installation, memory/boot state and outstanding warning](docs/reports/browser-softap-ota-installation.md)
- [OTA software verification and size/rollback report](docs/reports/browser-softap-ota-verification.md)
- [Commissioning, acceptance checklist and rollback](docs/commissioning-dual-p10.md)
- [Production upload and USB-only boot verification](docs/reports/dual-p10-flash-verification.md)
- [Historical software verification](docs/reports/dual-p10-software-verification.md)
- [Scoring](docs/specs/01-scoring-state-machine.md), [BLE](docs/specs/02-ble-protocol.md), [display](docs/specs/03-display-rendering.md) contracts
- Accepted diagnostic profiles remain in `hardware-tests/p10-bringup/`; they are not the production scoreboard.

**Installed hardware checkpoint:** owner-authorized `agent-wpa2` app-only install and all unrelated sectors verified. Cold AP-ready heap67,540, minimum67,436, largest internal byte block47,092; original thresholds unchanged. Full180-second timeout, nine BLE reactivations, five score/undo cycles and four actual WPA2/CCMP/DHCP/authenticated-HTTP sessions verified, with Ethernet and Wi-Fi restoration. [Exact installation, recovery and hardware results](docs/reports/agent-wpa2-hardware-verification.md).

**Latest endurance result: NOT lifecycle-cleared.** Fifty recorded authenticated cycles completed, but pre-network heap fell120,420→116,364 bytes; five closed minutes recovered only128 bytes. One HTTP-login transport failure and a separate serial-parser failure are retained, not counted as an uninterrupted50-cycle pass. Nine real pre-erase upload-rejection probes,179.96s timeout and final BLE score/undo passed. See [full measurements and pending authorization](docs/reports/ota-lifecycle-endurance.md).

**Still open:** SDK shutdown timer/resource attribution, full transfer/load, lit-panel and rollback-execution acceptance. Signed `agent-ota1` is not lifecycle-cleared; source-only diagnostic `agent-life1` adds return-preserving timer/allocator telemetry, not a speculative fix. **Neither package has been installed wirelessly. Another update, inactive-slot erase/write or normal OTA metadata change requires explicit authorization; do not deploy on resume.** [Earlier software diagnosis](docs/reports/ota-heap-admission-correction.md) and [historical wpa1 failure](docs/reports/agent-wpa1-installation.md) remain preserved.

## Build and test

```bash
# Repository root; Windows/Git Bash
PYTHONIOENCODING=utf-8 firmware/.venv/Scripts/pio.exe run -d firmware -e esp32dev
firmware/.venv/Scripts/python.exe scripts/run_firmware_host_tests.py --output-dir <external-session-artifacts>/host
firmware/.venv/Scripts/python.exe scripts/check_glyphs.py

export JAVA_HOME='/c/Program Files/Android/Android Studio/jbr'
cd android
./gradlew testDebugUnitTest assembleDebug lintDebug
# With an emulator: ./gradlew connectedDebugAndroidTest
```

See the commissioning guide for cross-language snapshots and this host's isolated Gradle test setup. **USB flashing/recovery requires fresh owner-confirmed isolation, no external 5 V and verified target identity. Routine signed wireless OTA does not use USB.** Software tests are not physical BLE/panel acceptance.

**Court 1 OTA:** the first signed wireless update reached authenticated `VALID` on `ota_1`. The second installed `court1-return-002` on `ota_0` with verified flash readback, UART trial confirmation, and independent authenticated HTTP verification of the exact running `VALID` image. Operational post-maintenance BLE recovery passed. AP shutdown evidence is consistent but not independently radio-verified. See the [deployment workflow](docs/agent-ota-deployment.md) and [second OTA evidence](docs/reports/ota-second-wireless-update.md). Both write approvals are spent. Do not seal before lifecycle, rollback and lit-panel acceptance. Recovery artifacts stay protected outside Git.

Scores and serving are set directly: `+`/`−` per team, and the serve key cycles L1 → L2 → R1 → R2. See the [scoring contract](docs/specs/01-scoring-state-machine.md).
