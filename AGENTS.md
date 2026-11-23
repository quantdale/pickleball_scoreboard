# AGENTS.md

## Preserve production

Read [README.md](README.md) for the current Court 1 checkpoint and the
[development guide](docs/development.md) for architecture and test commands.
The owner approved landscape gameplay; do not change scoring, layout, DMA
mapping/timing, heap floors, rollback policy or operator settings as cleanup.
[Specs 01–04](docs/specs) are authoritative; cite the governing section in
comments for spec-defined behavior. Historical reports and OpenSpec deltas
are not current contracts.

The latest authenticated record is `court-headroom-005b` (`ota_1`, VALID); the
valid fallback is `court-headroom-005` (`ota_0`). The prior portrait release is
preserved at the tag and archive branch `portrait-1.3.1-court-undo-003` and
`archive/portrait-1.3.1-court-undo-003`, respectively. Never infer live state or
fresh hardware permission from a report or Git ref.

## Hardware and trust

- Ordinary maintenance is offline. Do not activate the AP, connect to a real
  board, issue scoring commands, open serial, install an APK on an owner's
  phone, or write flash/NVS without separate authorization.
- For an authorized operation, follow [Mode A/Mode B](docs/agent-ota-deployment.md).
  Pin source/destination, exact signed package, expected downtime and recovery.
  Upload can erase the inactive slot even without install. Never retry an
  uncertain write or switch to USB automatically. USB service and identification
  require fresh isolation: panels unpowered and discharged, HUB75 disconnected
  while unpowered and no external 5 V at the USB-powered ESP32.
- Keep provisioning, owner credentials, private keys and complete flash images
  outside Git. Test keys are disposable and must never be provisioned.
- Scoring BLE intentionally remains open to one central (Spec 02 §8).
  Authenticated BLE maintenance grants a local OTA window, not firmware transfer
  over BLE (Spec 04 §8). Signed update verification is not hardware Secure Boot.
- `hardware-tests/p10-bringup/` is standalone, spare-only diagnostic code. A
  successful build is not authorization to flash it.

## Development workflow

- Run `openspec list --json` for active work. Before editing a change artifact,
  read `openspec status --change <name> --json` and
  `openspec instructions <artifact> --change <name> --json`. Before implementing
  a feature, read the `contextFiles` from `openspec instructions apply --change
  <name> --json`. Record verified task completion; don't tick superseded or
  hardware-gated work just to clear the list. Propose new behavior before coding
  it; Specs 01–04 take precedence over historical deltas.
- Keep firmware's existing C++ style and Kotlin's official style. Match local
  naming, add tests at existing seams, and avoid duplicate scoring logic in
  the production Android app. Update all cross-IDE skill copies together if a
  skill changes.
- Always select explicit PlatformIO targets. Native tests use `-e native`;
  bare `pio test` can wait for hardware. Keep build output out of Git. On the
  Windows development host, leave the global Gradle init file alone; see the
  isolated emulator setup in [development.md](docs/development.md).
- Preserve uncommitted work, unique history and checkpoints. Use an isolated
  worktree and a checked PR for integration. Keep evidence in the external
  session directory. CI, emulator tests and physical acceptance are distinct.

## UX, DX and AX

Prefer simple layouts and code. Cut taglines, redundant labels and helper text
rather than using them to explain a confusing layout. Improve the experience
for users, developers and agents without changing approved behavior.
