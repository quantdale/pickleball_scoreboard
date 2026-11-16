## Purpose

Commission the real scoreboard on one accepted horizontal pair of P10 panels with pixel-equivalent Android preview and unchanged owner-approved scoring.

## ADDED Requirements

### Requirement: Readable 64×16 score display

The scoreboard SHALL show left/right scores on corresponding halves of a 64×16 white-on-black canvas. Digits SHALL use the shared 5×7 font at 2× scale, producing 10×14 glyphs. Serving arrows SHALL occupy outer gutters; one arrow denotes server 1, two vertically stacked arrows denote server 2. Scores 0..99 SHALL not clip/overlap. Out-of-range values SHALL display two dashes without changing game state.

#### Scenario: Double-digit scores

- **WHEN** either side reports 10, 25 or 99
- **THEN** both digits fit within its own panel and the serving indication remains separate

#### Scenario: Preview equivalence

- **WHEN** firmware and Android render the same state
- **THEN** their complete logical pixel sets SHALL be identical, including arrow mirroring and out-of-range fallback

### Requirement: Commissioned hardware and state ownership

Firmware SHALL reuse accepted GPIOs, chain length 2, 1/8 scan, chain-block mapping and brightness 12/255. It SHALL show initial left-serving 0-0-2 and redraw after state changes, not continuously. ESP32 SHALL own game state; callbacks SHALL not race rendering.

#### Scenario: Boot and control

- **WHEN** the board boots or processes a valid state-changing BLE command
- **THEN** it displays the authoritative state using Spec 01 scoring, undo, switch, end, reset and singles/doubles semantics

#### Scenario: Physical acceptance boundary

- **WHEN** host tests and builds pass without owner observation
- **THEN** hardware E2E criteria remain unverified; flashing requires fresh isolated USB-only confirmation
