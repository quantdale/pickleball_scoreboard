## ADDED Requirements

### Requirement: Maintenance yields to gameplay
Browser maintenance SHALL NOT change BLE UUIDs, command bytes or state payload. A connected controller or queued scoring command SHALL prevent maintenance admission/installation. BLE activity SHALL close maintenance; acknowledged commands SHALL continue through the existing state-owning loop and SHALL NOT be silently discarded. Confirmed maintenance SHALL stop advertising while retaining the last score; timeout/cancel SHALL restore ordinary advertising without resetting state.

#### Scenario: Controller connects during the boot AP window
- **WHEN** BLE control begins before maintenance exclusivity
- **THEN** the AP SHALL shut down and normal scoring SHALL continue without a maintenance reboot

#### Scenario: Connection or command races maintenance admission
- **WHEN** BLE activity occurs while maintenance is being admitted
- **THEN** maintenance SHALL abort and accepted commands SHALL be processed before any possible install
