# Pickleball scoreboard

ESP32-owned scoring with an Android BLE remote and matching preview. Production display: **two horizontal 32×16 P10 panels (64×16), brightness 12/255**.

- [Agent deployment CLI and authenticated BLE activation](docs/agent-ota-deployment.md) — implemented; actual BLE/AP, Windows WPA/DHCP/admin HTTP verified. Firmware OTA itself awaits authorization and lifecycle acceptance. [Latest endurance evidence](docs/reports/ota-lifecycle-endurance.md).
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

See the commissioning guide for cross-language snapshots and this host's isolated Gradle test setup. **Do not flash without fresh owner-confirmed isolated USB-only setup, no external 5V and verified target identity.** Software tests are not physical BLE/panel acceptance.

**Court 1 OTA:** one authorized signed wireless update installed `agent-life2-budget` on `ota_1` and reached `VALID`. The next exact package is prepared but not authorized or staged. See the [deployment workflow](docs/agent-ota-deployment.md) and [acceptance gates](docs/reports/ota-hardening-checkpoint.md). Do not seal before repeat OTA, rollback, resource and lit-panel acceptance. Original migration/recovery artifacts stay protected outside Git, and any future serial reset requires fresh isolation and target verification.

The approved doubles **full side-out sets server 2**, not standard doubles server 1. This rule is preserved; the owner must confirm gameplay acceptance before a real match.
