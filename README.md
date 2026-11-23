# PickleScore

An ESP32 controls two 32×16 P10 panels as one 64×16 landscape scoreboard. The
Android 2.2.0 controller displays the board's state over BLE and sends manual
score and serve inputs. Doubles serving cycles L1 → L2 → R1 → R2; Singles
alternates sides. Switch Court, End Game/Resume, Reset, uniform LED color and
persistent brightness are available from the controller.

## Court 1 production baseline

The owner approved landscape gameplay. The latest authenticated deployment
record is **`court-headroom-005b` on `ota_1`**, with **`court-headroom-005` on
`ota_0`** as fallback. See the [deployment and heap evidence](docs/reports/ota-reliability-followup.md).
This is a recorded checkpoint, not a live status probe or authorization to
redeploy. Failed-trial rollback on hardware, repeat OTA on the landscape build,
endurance and electrical certification above brightness level 1 are separate.
The [portrait release tag](https://github.com/quantdale/pickleball_scoreboard/tree/portrait-1.3.1-court-undo-003)
and dedicated archive branch preserve recovery source; portrait is not the
current on-device fallback.

- [Landscape operator guide](docs/landscape-operator-guide.md) — controls, panel orientation, compatibility and recovery.
- [Development and offline verification](docs/development.md) — toolchains, builds, tests and repository layout.
- [Installed OTA and recovery](docs/agent-ota-deployment.md) — separately authorized operations; no automatic USB fallback.
- [Specifications](docs/specs): [scoring](docs/specs/01-scoring-state-machine.md), [BLE](docs/specs/02-ble-protocol.md), [display and wiring](docs/specs/03-display-rendering.md), [OTA](docs/specs/04-local-browser-ota.md).
- [Engineering history](docs/reports/README.md) — release evidence, older commissioning and diagnostic reports.
