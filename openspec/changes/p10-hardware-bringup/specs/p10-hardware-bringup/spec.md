## Purpose

Provide isolated, auditable LED-panel commissioning before connecting production scoreboard logic or commissioning a second panel.

## ADDED Requirements

### Requirement: Isolated low-brightness color cycle

The diagnostic SHALL default to one 32×16 physical panel using the owner's explicit GPIO map and low brightness. It SHALL repeat red, green, blue, white, and black at two-second intervals, independently of BLE, Android, and scoring.

#### Scenario: Normal execution

- **WHEN** initialization succeeds
- **THEN** the display starts black and the repeating five-color sequence is logged over serial at 115200 baud

#### Scenario: Initialization failure

- **WHEN** display initialization reports failure
- **THEN** the diagnostic logs the error and does not enter its color cycle

#### Scenario: Disconnected panel

- **WHEN** the panel is disconnected
- **THEN** the firmware does not wait for panel detection and does not treat serial success as physical display verification

### Requirement: Two-panel horizontal geometry diagnostic

After the owner's single-panel visual pass, the explicitly selected two-panel diagnostic SHALL use two 32×16 1/8-scan modules as one 64×16 horizontal canvas at brightness 12/255, preserving the supplied GPIO map. Each repeating sequence SHALL include full-screen red/green/blue/white/black, independently colored 32-pixel halves, an asymmetric coordinate grid, and column/row sweeps. Serial output SHALL identify each phase and sweep coordinate. Logical left and right halves SHALL be distinguished from unverified physical cable order.

#### Scenario: Independent halves

- **WHEN** the two-panel diagnostic reaches its independent color phases
- **THEN** logical x=0..31 is red and x=32..63 is blue, followed by green and white respectively

#### Scenario: Pixel geometry

- **WHEN** the geometry phases run
- **THEN** the canvas shows panel-half labels and orientation markers, then a single white column advances from x=0 to x=63 and a single white row advances from y=0 to y=15 on black

#### Scenario: Single-panel regression protection

- **WHEN** the single-panel environment is built
- **THEN** its original five-color cycle remains unchanged and no two-panel patterns run

### Requirement: Pixel-boundary validation profile

A separately selected two-panel boundary profile SHALL preserve 64×16 geometry, GPIOs, driver timing and brightness 12/255. It SHALL clear the full canvas before every frame and confine each logical panel to its own 32×16 extent. Static frames SHALL be held for five seconds; the complete sequence SHALL repeat with serial phase/coordinate diagnostics.

#### Scenario: Borders and fills

- **WHEN** patterns A and B are shown
- **THEN** A shows Panel 1 red border/blue interior and Panel 2 yellow border/cyan interior; B shows Panel 1 green border/purple interior and Panel 2 red border/yellow interior, all borders exactly one pixel thick

#### Scenario: Corners and isolation

- **WHEN** pattern C frames run
- **THEN** each panel's four corners are distinguishable; x=0,31,32,63 have identifiable vertical markers; x=31 and x=32 are shown individually and together; each panel is shown alone while the other is black

#### Scenario: Address validation and reproduction

- **WHEN** address-validation frames run
- **THEN** normal and inverted single-pixel checkerboards, white columns x=0..63, white rows y=0..15 and the original Panel 1 green/Panel 2 white case expose missing, duplicated or shifted coordinates without assuming the reported green strip is harmless

### Requirement: Physical safety and staged commissioning

Flashing SHALL wait for the owner's isolation and device-identity confirmation. The two-panel mode SHALL remain a separately selected build and SHALL NOT be physically tested until the owner confirms Panel 1 passed. Physical power-on SHALL wait for connector, ground, protected DC wiring, and enclosed mains safety checks.

#### Scenario: Unconfirmed isolation

- **WHEN** the owner has not confirmed PSU isolation, unpowered signal disconnection, nonconductive support, USB-only controller power, and the selected serial device
- **THEN** software preparation proceeds without uploading or opening the serial port

#### Scenario: Two-panel preparation

- **WHEN** the two-panel build succeeds but Panel 1 has not passed visually
- **THEN** that build is retained for later use without claiming physical geometry or scan compatibility
