# Pickleball scoreboard

ESP32-owned scoring with an Android BLE remote and matching preview. The baseline on `main` uses two horizontal 32×16 P10 panels (64×16), brightness 12/255.

The selected development baseline for the Court 1 portrait scoreboard and PickleScore 1.3.1 controller is [`fix/stable-controller-status`](https://github.com/quantdale/pickleball_scoreboard/tree/fix/stable-controller-status). Changes can land on `main` independently of wireless deployment. The full branch remains separate until failed-trial rollback, sustained-load testing, and owner-observed display acceptance are complete. A merge into `main` is not permission to flash any board.

- [Local SoftAP/browser OTA: provisioning, security, migration and two-update guide](docs/ota-maintenance.md)
- [Actual OTA installation, memory/boot state and outstanding warning](docs/reports/browser-softap-ota-installation.md)
- [OTA software verification and size/rollback report](docs/reports/browser-softap-ota-verification.md)
- [Commissioning, acceptance checklist and rollback](docs/commissioning-dual-p10.md)
- [Production upload and USB-only boot verification](docs/reports/dual-p10-flash-verification.md)
- [Historical software verification](docs/reports/dual-p10-software-verification.md)
- [Scoring](docs/specs/01-scoring-state-machine.md), [BLE](docs/specs/02-ble-protocol.md), [display](docs/specs/03-display-rendering.md) contracts
- Accepted diagnostic profiles remain in `hardware-tests/p10-bringup/`; they are not the production scoreboard.

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

**OTA-A is installed and privately provisioned after approved USB migration.** Profiles remain opt-in (`ota`, `ota_b`); no default credentials/private signing key are built into firmware. Full backup/recovery and signed A/B packages are preserved outside Git. Wireless updates and rollback execution remain untested; an idle Wi-Fi shutdown warning is documented. Do not seal before two owner-observed wireless updates, recovery and normal scoring tests pass. Any future serial work requires fresh isolation if the setup changes.

The approved doubles **full side-out sets server 2**, not standard doubles server 1. This rule is preserved; the owner must confirm gameplay acceptance before a real match.
