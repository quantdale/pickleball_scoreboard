## ADDED Requirements

### Requirement: Subscription-time authoritative state sync

The ESP32 SHALL send current state after the central enables the state CCCD, not rely on a notification before subscription. Android SHALL not enable controls until both subscription confirmation and a valid initial state have arrived. UUIDs, Notify-only state characteristic, command format and six-field payload SHALL remain unchanged.

#### Scenario: First connection or reconnect

- **WHEN** a central subscribes to state notifications
- **THEN** the current in-memory state is sent without resetting the game or requiring a scoring command

#### Scenario: Callback ordering

- **WHEN** initial state arrives before or after descriptor-write confirmation
- **THEN** Android becomes ready exactly once, only after both conditions are met

#### Scenario: Failed or stale connection

- **WHEN** discovery/subscription fails, connection is lost, or an old connection emits callbacks
- **THEN** readiness and pending writes are cleared; stale callbacks do not control the new connection

### Requirement: Serialized command writes

Android SHALL serialize acknowledged command writes and preserve each three-byte reset as one atomic command. Invalid firmware commands SHALL not change state or issue a state-change notification.

#### Scenario: Rapid button presses

- **WHEN** several commands are submitted before prior writes finish
- **THEN** they are sent in order with at most one command write outstanding
