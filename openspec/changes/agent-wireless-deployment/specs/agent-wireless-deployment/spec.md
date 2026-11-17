## Purpose

Enable an explicitly authorized coding agent to deploy signed scoreboard firmware over local Wi-Fi without manual network or browser interaction while protecting normal scoring and recovery.

## ADDED Requirements

### Requirement: Explicit authorized deployment with honest results
The CLI SHALL require explicit score-loss authorization, verify signed package compatibility and target identity, distinguish build/sign/transfer/install/reboot/health stages, and refuse success until expected version, slot and health are verified. It MUST NOT silently use USB flashing or infer idle from0-0.

#### Scenario: Upload is not deployment
- **WHEN** transfer completes but boot confirmation fails
- **THEN** deployment reports failure or unverified health, not success

### Requirement: Preserve Windows networking
The CLI SHALL use Windows WLAN control, keep and verify Ethernet internet routing/connectivity, bind local HTTP to the Wi-Fi address and restore the prior Wi-Fi profile/radio state after success or failure. Credentials MUST NOT appear in logs or command-line arguments.

#### Scenario: Failure during upload
- **WHEN** upload is interrupted
- **THEN** the result distinguishes the failure and restores prior Wi-Fi state without selecting an incomplete image

### Requirement: Authenticated bounded maintenance activation
Firmware SHALL provide a dedicated BLE challenge-response mechanism using the provisioned device-specific maintenance secret, fresh single-use expiring challenges and bounded failed attempts. It SHALL bind proof to target and explicit score-loss authorization, wait for requester disconnection and reject intervening scoring activity. Existing scoring UUIDs, commands, queue semantics and rendering SHALL remain unchanged.

#### Scenario: Replay or wrong authorization
- **WHEN** a stale, consumed, altered, unauthorized or wrong-device proof is received
- **THEN** no maintenance activation is scheduled

#### Scenario: Controller occupies BLE
- **WHEN** a scoring controller is connected
- **THEN** the agent does not forcibly disconnect it or activate maintenance through scoring commands

### Requirement: Repeated maintenance and recovery evidence
Firmware SHALL support closing and reactivating bounded Wi-Fi maintenance without USB, expose confirmed startup state to authenticated deployment verification, and preserve inactive-slot signing/readback/rollback protections. Real-device and mocked evidence MUST remain distinguishable.

#### Scenario: Complete wireless lifecycle
- **WHEN** startup AP has expired and an authorized BLE proof is accepted
- **THEN** the agent connects Wi-Fi, deploys to the inactive slot, verifies reboot/health, restores networking and can repeat without USB flashing
