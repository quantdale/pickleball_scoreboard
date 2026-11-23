## Purpose

Lets the operator choose one color for every lit LED from the controller,
independently of scoring and brightness.

## ADDED Requirements

### Requirement: Palette
The firmware SHALL drive every lit pixel in one color from White, Red, Yellow, Green, Cyan and Blue, each turning whole RGB channels fully on or off. White SHALL be the default.

#### Scenario: Red
- **WHEN** the color is Red
- **THEN** every lit digit and arrow pixel is driven as RGB565 `0xF800` and every other pixel is black

### Requirement: Color command and report
The firmware SHALL accept `K` followed by one palette letter, repaint once, and notify `K,<code>` on the Manual State stream after the brightness report on initial synchronization and after each change. Any other `K` write SHALL change nothing and notify nothing.

#### Scenario: Valid color
- **WHEN** the board in White receives `KR`
- **THEN** it repaints in Red and notifies `K,R`

#### Scenario: Unknown color
- **WHEN** the board receives `KM` or `Kr`
- **THEN** nothing changes and nothing is notified

### Requirement: Independence and persistence
A color command SHALL NOT change game state, history or brightness, and no scoring input SHALL change the color. The color SHALL be stored in NVS with brightness after 3 s without change, only if it differs, never during maintenance, and restored before the first frame; an unknown record restores White.

#### Scenario: Restart
- **WHEN** the color was Green for more than 3 s and the board restarts
- **THEN** the boot frame is drawn in Green

### Requirement: Controller selector
The Android Board menu SHALL show one swatch per palette color only while synchronized and only after a color report, mark only the reported color, and keep at most one unsent color write that yields to scoring writes.

#### Scenario: Tap before the report
- **WHEN** the operator taps Red while the board reports White
- **THEN** `KR` is sent and White stays marked until the board reports `K,R`
