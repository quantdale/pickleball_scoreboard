# MT Pickle Park scoreboard

An ESP32 controls two 32×16 P10 panels as one 64×16 landscape scoreboard. The
MT Pickle Park Android 2.3.1 controller displays the board's state over BLE and
sends manual score and serve inputs. Doubles serving cycles L1 → L2 → R1 → R2;
Singles alternates sides. Switch Court, Reset, brightness and LED color are
available on the playing screen. End Game and Resume are in the Board menu.
The board still advertises as `PickleScore`.

## Court 1 production baseline

The owner approved landscape gameplay and accepted the faint close-range pixel
imperfection on the physical panels, freezing the current firmware, display
implementation, and approved functionality for production. The latest deployed
Court 1 record is **`court-hardened-006` on `ota_1`** (installed via Mode B
application-only USB write, whole-chip verified), with
**`court-headroom-005` on `ota_0`** preserved as fallback, paired with the
release-signed **Android `2.2.1`** (`versionCode 10`) APK. See the
[USB hardening and release report](docs/reports/court1-usb-hardening-006.md) and
prior
[wireless OTA deployment and heap evidence](docs/reports/ota-reliability-followup.md).
This is a recorded checkpoint, not a live status probe or authorization to
redeploy. Live wireless OTA on `-006`, failed-trial hardware rollback,
20-minute endurance, electrical certification above brightness level 1, and
physical phone controller E2E are deferred hardware qualification checks, not
production blockers.
The [portrait release tag](https://github.com/quantdale/pickleball_scoreboard/tree/portrait-1.3.1-court-undo-003)
and dedicated archive branch preserve recovery source; portrait is not the
current on-device fallback.

- [Landscape operator guide](docs/landscape-operator-guide.md) — controls, panel orientation, compatibility and recovery.
- [Development and offline verification](docs/development.md) — toolchains, builds, tests and repository layout.
- [Installed OTA and recovery](docs/agent-ota-deployment.md) — separately authorized operations; no automatic USB fallback.
- [Specifications](docs/specs): [scoring](docs/specs/01-scoring-state-machine.md), [BLE](docs/specs/02-ble-protocol.md), [display and wiring](docs/specs/03-display-rendering.md), [OTA](docs/specs/04-local-browser-ota.md).
- [Engineering history](docs/reports/README.md) — release evidence, older commissioning and diagnostic reports.
