## Why

The installed scoreboard needs local firmware maintenance without USB or an external router. The owner explicitly authorizes implementation of browser-based SoftAP OTA, while retaining scoring/display/BLE behavior and requiring honest recovery limits.

## What Changes

- Add a provisioned WPA2 SoftAP boot window, separate maintenance authentication, bounded sessions and a tiny offline browser updater.
- Require P-256 signed update packages, strict size/target/image validation and inactive-slot writes; upload only stages, explicit installation selects boot/reboots.
- Give BLE gameplay priority: a controller connection closes maintenance; authenticated maintenance stops/acknowledges advertising only with no connected controller or pending commands. Never discard acknowledged scoring commands.
- **BREAKING (initial installation only):** widen both OTA slots to0x1f0000 using an explicit partition migration. No automatic migration, erase, bootloader replacement or eFuse changes.
- Defer Arduino's early confirmation and confirm pending applications only after startup checks; retain actual supported bootloader rollback. Prepare signing/provisioning tools, automated failure tests and two-update physical commissioning instructions.

## Capabilities

### New Capabilities

- `local-browser-ota`: Provisioning, local authenticated maintenance, signed inactive-slot OTA, rollback and commissioning gates.

### Modified Capabilities

- `ble-protocol`: Maintenance temporarily suspends advertising only without a central; gameplay takes priority and the command/state wire format stays unchanged.

## Impact

Firmware main/BLE adapters, new bounded maintenance module, OTA partition/build profile, offline packaging/provisioning tools and focused tests/docs. Android/scoring/rendering rules remain unchanged. The working baseline stays on its existing branch; no hardware operation or remote push is authorized by software completion.
