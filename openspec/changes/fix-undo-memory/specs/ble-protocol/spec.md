## ADDED Requirements

### Requirement: Scoring write acknowledgment follows queue admission
The ESP32 SHALL enqueue a valid scoring command before sending ATT success. A full command queue SHALL return ATT insufficient resources without modifying the game or dropping an older accepted command. ATT success is transport only; journal refusal is still reported on the extended state stream.

#### Scenario: Queue saturated when a Reset write arrives
- **WHEN** a valid Reset Write Request arrives while the command queue is full
- **THEN** the board returns an ATT insufficient resources error and preserves the pending commands in order

#### Scenario: Disconnect after a valid write
- **WHEN** a valid scoring command enters the queue and the central disconnects
- **THEN** the state-owning loop still processes the command and the next subscription sees the resulting authoritative state
