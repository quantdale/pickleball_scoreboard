## Purpose

Keep the landscape controller branded for MT Pickle Park and usable courtside without opening board management for routine LED adjustments.

## ADDED Requirements

### Requirement: Controller branding
The launcher label and controller branding SHALL identify MT Pickle Park using the supplied logo artwork. The application ID, signing identity and firmware compatibility SHALL remain unchanged.

#### Scenario: Upgrade from 2.2.1
- **WHEN** the signed 2.3.0 APK replaces the signed 2.2.1 APK
- **THEN** Android accepts an in-place update and shows the new name and artwork.

### Requirement: Courtside settings
The landscape control surface SHALL provide board-reported brightness and direct access to the LED palette, without moving the primary score and serve controls or exposing controls for unreported settings.

#### Scenario: Apply a setting
- **WHEN** the operator steps brightness or picks a color on a synchronized board
- **THEN** the controller requests the change and updates the displayed setting only when the board reports it.

### Requirement: Secondary game actions
End Game and Resume SHALL remain available in the Board menu rather than the playing surface. Switch Court and Reset SHALL remain on the playing surface.

#### Scenario: Game is ended
- **WHEN** a synchronized board reports a finished game with Undo available
- **THEN** the Board menu offers Resume, which requests one Undo per ended state.
