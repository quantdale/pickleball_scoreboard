# BLE delta

## ADDED Requirements

### Requirement: Optional authoritative undo status
The ESP32 SHALL offer the complete score and Undo status together in an optional Notify characteristic without changing the six-field legacy characteristic or command bytes.

#### Scenario: Reconnect with history
- **WHEN** a client subscribes to the extended stream after reconnect
- **THEN** the initial state includes the current Undo status without changing scores or history

#### Scenario: Legacy interoperability
- **WHEN** an older client subscribes to the legacy stream or a new controller discovers legacy-only firmware
- **THEN** legacy scoring and Undo commands still work and the legacy stream remains six fields
