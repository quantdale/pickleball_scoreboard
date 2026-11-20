## Why

Players who tried PickleScore on court found rally-winner scoring hard to operate
mid-match: correcting a wrong score meant replaying the serving history. The owner
asked for a landscape board and controller that anyone can run by setting the
scores and the serving arrow directly.

## What Changes

- **BREAKING** Scoring becomes manual (Spec 01): `+`/`−` per team (0–99), a serve
  indicator cycling L1 → L2 → R1 → R2 (Singles L1 ↔ R1), Switch Court, End Game,
  Reset(mode) and full-position Undo. Rally and side-out logic is removed.
- **BREAKING** BLE commands become `+L -L +R -R S C E U 0D 0S` (Spec 02). Portrait
  rally and three-byte reset commands are malformed. A Manual State stream
  (`...26ac`) replaces the portrait extended stream (`...26ab`); Legacy State
  (`...26a9`) keeps its format for the OTA tooling.
- **BREAKING** The board renders LEFT SCORE | SERVING ARROWS | RIGHT SCORE on a
  64×16 landscape canvas (Spec 03), with selectable physical mounts.
- Undo history is a fixed 2,048-position ring in the existing 6,144 B journal;
  the oldest entry drops instead of refusing a correction.
- Android 2.0.0 is landscape-only and follows the owner's sketch.
- The portrait release is preserved at tag `portrait-1.3.1-court-undo-003`.

## Capabilities

### New Capabilities
- `landscape-controller`: the Android landscape controller layout, serve key, confirmations and connection gating.

### Modified Capabilities
- `ble-protocol`: the manual command set, Manual State stream and portrait-firmware refusal replace the three-byte reset.
- `scoring-state-machine-tests`: test coverage follows the manual scoring model.
- `shared-display-assets`: the serving arrow becomes a 6×12 triangle and the layout becomes landscape.

## Impact

- Firmware: `game_state`, `ble_command_parser`, `ble_service`, `state_payload`,
  `display_render_logic`, `display_render`, `p10_hardware` and all scoring,
  parser, display and protocol tests. OTA, maintenance and memory code unchanged.
- Android: BLE client, parser, state, screen, pixels and tests; version 2.0.0 (7).
- Shared assets, pixel and scoring fixtures, glyph/snapshot checkers.
- `tools/ota_deploy.py` serial witness accepts the landscape boot position.
- Controller and board must be updated together; mixed versions refuse or ignore
  each other's commands rather than misinterpreting them.
