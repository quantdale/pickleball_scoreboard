# Scoring tests delta

## ADDED Requirements

### Requirement: Full history reversal coverage
Both scoring suites SHALL verify long mixed sequences can be reversed to the initial position in both modes, including serving and opening state, Ends and Resets.

#### Scenario: Complete unwind
- **WHEN** accepted actions are undone in reverse order
- **THEN** each prior position is restored and further Undo at the bottom has no effect
