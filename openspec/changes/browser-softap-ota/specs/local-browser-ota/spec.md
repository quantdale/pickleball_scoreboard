## Purpose

Permit sealed PickleScore scoreboards to receive authenticated, signed firmware updates over their own local Wi-Fi network without changing ordinary scoring behavior.

## ADDED Requirements

### Requirement: Provisioned bounded local maintenance
The device SHALL expose a WPA2-protected local-only AP at192.168.4.1 for180 seconds after a provisioned boot. It SHALL have no default credentials, router dependency, captive portal or internet request. An authenticated session SHALL expire after600 seconds including transfer time; unauthenticated requests SHALL NOT extend the window. Missing/invalid provisioning SHALL leave OTA disabled and ordinary BLE available.

#### Scenario: No owner maintenance session
- **WHEN** the window expires or a BLE controller connects
- **THEN** maintenance SHALL close, release Wi-Fi resources and preserve game state without reboot

### Requirement: Separate authorization and signed firmware
Provisioning SHALL supply a unique32-character AP password, independent high-entropy maintenance secret and trusted P-256 public key, with no secrets committed to Git. State-changing HTTP requests SHALL require authorization, exact local Host/Origin, and a single-use unpredictable CSRF/request nonce. Firmware SHA256 SHALL be bound into an offline P-256 signature, not accepted as authorization alone.

#### Scenario: Unauthorized or replayed request
- **WHEN** authentication, origin, nonce or signature validation fails
- **THEN** the device SHALL reject the operation and SHALL NOT select a new boot image

### Requirement: Inactive-slot staged installation
Uploads SHALL write only a validated inactive OTA application partition and enforce exact package/image sizes, target compatibility, image format, signature, payload hash, image validity and flash readback hash. Upload SHALL stage only; a separate explicit install confirmation SHALL select the next boot partition and reboot only when BLE control is absent and maintenance exclusivity is confirmed.

#### Scenario: Transfer failure
- **WHEN** an upload is invalid, oversized, truncated, interrupted, duplicated, concurrent or expired
- **THEN** it SHALL fail without overwriting the running application or selecting incomplete firmware

### Requirement: Genuine boot validation and recovery
The implementation SHALL verify actual bootloader rollback capability. Pending OTA images SHALL remain unconfirmed until display/scoring/BLE and provisioned maintenance startup checks pass. Initial USB partition migration SHALL require fresh physical isolation AND explicit migration authorization. No eFuse or bootloader replacement SHALL occur automatically.

#### Scenario: Updated application fails before confirmation
- **WHEN** the supported rollback bootloader observes a reset of an unconfirmed application
- **THEN** it SHALL revert to the previously valid application; hardware proof SHALL remain pending until owner commissioning

### Requirement: Honest commissioning evidence
Software tests SHALL distinguish fake-backend/host checks from actual radio/display acceptance. The owner SHALL demonstrate two consecutive distinguishable browser updates, interrupted/invalid rejection and continuing BLE/panel function before physical acceptance.

#### Scenario: Reboot after explicit installation
- **WHEN** a valid staged update is explicitly installed
- **THEN** the device SHALL reboot into the updated version and explicitly reset RAM-only score to0-0-2; no score persistence SHALL be promised
