# Pickleball scoreboard

ESP32-owned scoring with an Android BLE remote and matching preview. Production display: **two horizontal 32×16 P10 panels (64×16), brightness 12/255**.

- [Agent deployment CLI and authenticated BLE activation](docs/agent-ota-deployment.md) — implemented; wireless commissioning blocked by the latest AP startup failure and earlier Windows WPA failure. [Commissioning evidence](docs/reports/agent-wireless-deployment-verification.md).
- [Local SoftAP/browser OTA: provisioning, security, migration and two-update guide](docs/ota-maintenance.md)
- [Actual OTA installation, memory/boot state and outstanding warning](docs/reports/browser-softap-ota-installation.md)
- [OTA software verification and size/rollback report](docs/reports/browser-softap-ota-verification.md)
- [Commissioning, acceptance checklist and rollback](docs/commissioning-dual-p10.md)
- [Production upload and USB-only boot verification](docs/reports/dual-p10-flash-verification.md)
- [Historical software verification](docs/reports/dual-p10-software-verification.md)
- [Scoring](docs/specs/01-scoring-state-machine.md), [BLE](docs/specs/02-ble-protocol.md), [display](docs/specs/03-display-rendering.md) contracts
- Accepted diagnostic profiles remain in `hardware-tests/p10-bringup/`; they are not the production scoreboard.

**Latest commissioning stop:** authorized `agent-wpa1` application-only install verified, but OTA/AP startup self-check failed; maintenance is disabled. No WPA/DHCP/HTTP pass or wireless update is claimed. [Current hardware state and recovery evidence](docs/reports/agent-wpa1-installation.md).

**Prepared, not installed:** signed `agent-wpa2` reclaims unused Classic controller RAM for BLE-only OTA builds, retains cache4/heap safeguards and adds stage/fragmentation diagnostics.86 firmware,29 CLI,15 signing,42 Android tests and48 snapshots pass. [Heap diagnosis, exact candidate and remaining physical gates](docs/reports/ota-heap-admission-correction.md). Fresh isolation and explicit app-only candidate authorization are required before flashing.

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

**Earlier feature-branch device: `agent-v2-a`/`ota_0`, explicitly USB-app-installed with existing provisioning/layout.** Authenticated BLE activation and repeated scoring/AP cycles work; actual Wi-Fi firmware deployment remains unproven. See the agent commissioning report for the corrected cleanup stack and WPA blocker.

**Original OTA-A was installed and privately provisioned after approved USB migration.** Profiles remain opt-in (`ota`, `ota_b`); no default credentials/private signing key are built into firmware. Full backup/recovery and signed A/B packages are preserved outside Git. Wireless updates and rollback execution remain untested; an idle Wi-Fi shutdown warning is documented. Do not seal before two owner-observed wireless updates, recovery and normal scoring tests pass. Any future serial work requires fresh isolation if the setup changes.

The approved doubles **full side-out sets server 2**, not standard doubles server 1. This rule is preserved; the owner must confirm gameplay acceptance before a real match.
