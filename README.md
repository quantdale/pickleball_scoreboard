# Pickleball scoreboard

ESP32-owned scoring with an Android BLE remote and matching preview. Production display: **two horizontal 32×16 P10 panels (64×16), brightness 12/255**.

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

The approved doubles **full side-out sets server 2**, not standard doubles server 1. This rule is preserved; the owner must confirm gameplay acceptance before a real match.
