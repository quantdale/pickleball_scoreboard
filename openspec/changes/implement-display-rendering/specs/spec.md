# display-rendering Specification

## Purpose

Define the 64×32 pixel layout, colors, and rendering behavior for the pickleball scoreboard on both the ESP32 firmware and the Android app preview. The authoritative source of truth is `docs/specs/03-display-rendering.md`; this delta spec captures the same requirements in OpenSpec format for the `implement-display-rendering` change.

## ADDED Requirements

### Requirement: Logical canvas size

The system SHALL render on a 64 pixel wide × 32 pixel high logical canvas, composed of four 32×16 panels arranged in a 2×2 grid.

#### Scenario: Firmware canvas matches physical panel

- **WHEN** the firmware initializes the HUB75 driver
- **THEN** it SHALL configure a 64×32 logical canvas with the panel arrangement top-left, top-right, bottom-left, bottom-right.

#### Scenario: Android preview preserves aspect ratio

- **WHEN** the Android app displays the scoreboard preview
- **THEN** it SHALL render a 64×32 logical grid scaled to fit the available width while preserving the 2:1 aspect ratio.

### Requirement: Color scheme

Lit content SHALL be white (`#FFFFFF`); unlit pixels SHALL be black (`#000000`). Arrows SHALL be the same white color as score digits, not green.

#### Scenario: Lit pixels are white

- **GIVEN** any rendered scoreboard state
- **WHEN** a pixel belonging to a digit, arrow, or divider is lit
- **THEN** its color SHALL be white.

#### Scenario: Background is black

- **GIVEN** any rendered scoreboard state
- **WHEN** a pixel is not part of any lit content
- **THEN** it SHALL be black/unlit.

### Requirement: Score digit rendering

The system SHALL render score digits using the 5×7 bitmap font defined in `shared/display_assets/font_5x7.json`, with a 1-pixel gap between adjacent digits.

#### Scenario: Single-digit score centered in half

- **GIVEN** a side score of `0`
- **WHEN** the digit is rendered
- **THEN** it SHALL be horizontally centered within its half of the canvas.

#### Scenario: Two-digit score centered in half

- **GIVEN** a side score of `10`
- **WHEN** the digits are rendered
- **THEN** the combined glyph block SHALL be horizontally centered within its half of the canvas.

### Requirement: Server arrow rendering

The system SHALL render server arrows using the 9×7 `ARROW_RIGHT` bitmap defined in `shared/display_assets/arrows.json`. `ARROW_LEFT` SHALL be derived at runtime by horizontally reversing each row of `ARROW_RIGHT`.

#### Scenario: Server number 1 shows one arrow

- **GIVEN** `serverNumber == 1`
- **WHEN** the serving side is rendered
- **THEN** exactly one arrow pointing toward the serving side SHALL appear above that side's score.

#### Scenario: Server number 2 shows two arrows

- **GIVEN** `serverNumber == 2`
- **WHEN** the serving side is rendered
- **THEN** two identical arrows pointing toward the serving side SHALL appear side by side with a 1-pixel gap above that side's score.

### Requirement: Center divider

The system SHALL render a thin vertical divider at the horizontal midpoint of the canvas, spanning the content area.

#### Scenario: Divider is dim

- **GIVEN** a rendered scoreboard state
- **WHEN** the divider is drawn
- **THEN** it SHALL be rendered as a sparse/dotted line so it appears dimmer than the score and arrow content.

### Requirement: Rendering trigger

Both displays SHALL re-render whenever the displayed state changes.

#### Scenario: Firmware redraws on state change

- **GIVEN** the firmware receives a BLE command that changes the game state
- **WHEN** `renderState()` is called
- **THEN** it SHALL redraw the canvas to reflect the new state.

#### Scenario: Android redraws on Notify payload

- **GIVEN** the Android app receives a new state Notify payload
- **WHEN** the preview composable recomposes
- **THEN** it SHALL redraw the canvas to reflect the new state.

### Requirement: Cross-platform pixel equivalence

For the same game state, the firmware physical panel and the Android app preview SHALL light the same pixels in the same positions.

#### Scenario: 0-0-2 left serving matches on both displays

- **GIVEN** a state of left score `0`, right score `0`, serving side `LEFT`, server number `2`
- **WHEN** both displays render it
- **THEN** both SHALL show two left arrows, two zero digits, and the center divider in identical positions.
