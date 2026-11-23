## Purpose

Enable an explicitly authorized coding agent to deploy signed scoreboard firmware over local Wi-Fi without manual network or browser interaction while protecting normal scoring and recovery.

## ADDED Requirements

### Requirement: Explicit authorized deployment with honest results
The CLI SHALL require explicit score-loss authorization, the separately approved prebuilt signed package digest, and the exact approved source build/slot/image digest in the confirmed VALID state. It SHALL verify compatibility and target identity and upload the immutable bytes it verified. It SHALL distinguish build/sign/transfer/install/reboot/health stages and refuse success until exact staged extent, expected version, slot and health are verified. It MUST NOT use USB as a live deployment path or infer idle from 0-0.

#### Scenario: Approved source or package does not match
- **WHEN** the package or live source no longer matches the owner's approved transaction, or the live source is not confirmed VALID and healthy
- **THEN** the CLI refuses to upload any bytes or change the inactive slot

#### Scenario: Upload is not deployment
- **WHEN** transfer completes but boot confirmation fails
- **THEN** deployment reports failure or unverified health, not success

### Requirement: Preserve Windows networking
The CLI SHALL use Windows WLAN control, keep and verify Ethernet internet routing/connectivity, bind local HTTP to the Wi-Fi address and restore the prior Wi-Fi profile/radio state after success or failure. Credentials MUST NOT appear in logs or command-line arguments.

#### Scenario: The pinned AP associates but DHCP does not complete
- **WHEN** the exact WPA2/CCMP AP is connected but a preferred DHCP lease does not appear within the bounded lease phase
- **THEN** Windows may use a temporary on-link IPv4 address after collision checks and duplicate address detection, without changing DHCP policy, gateway, DNS or Ethernet routing
- **AND** source/interface, on-link route, expected peer MAC and HTTP session checks still gate the upload; the temporary state is removed and verified on exit

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
Firmware SHALL support closing and reactivating bounded Wi-Fi maintenance without USB, expose confirmed startup state to authenticated deployment verification, and preserve inactive-slot signing/readback/rollback protections. Wi-Fi teardown SHALL observe default netif AP stop before deinitialization and fail closed if teardown cannot finish. Real-device and mocked evidence MUST remain distinguishable.

#### Scenario: Complete wireless lifecycle
- **WHEN** startup AP has expired and an authorized BLE proof is accepted
- **THEN** the agent connects Wi-Fi, deploys to the inactive slot, verifies reboot/health, restores networking and can repeat without USB flashing
