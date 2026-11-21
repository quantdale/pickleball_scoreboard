## Purpose

Keeps wireless updates dependable without lowering any device memory floor.

## ADDED Requirements

### Requirement: Static headroom
The firmware SHALL keep at most 32 Undo positions so the OTA build's static DRAM stays at or below 63,000 B, and the Android controller SHALL offer no Undo key, sending Undo only as a single Resume per ended-game report.

#### Scenario: Double tap on Resume
- **WHEN** the operator taps Resume twice before the board reports
- **THEN** only one Undo is sent

### Requirement: No per-session timer leak
The firmware SHALL stop and delete a timer whose deletion is refused because it is armed, and SHALL report armed deletions and recoveries in authenticated status.

#### Scenario: Maintenance close
- **WHEN** Wi-Fi teardown deletes an armed timer
- **THEN** the timer is stopped and freed and `timerRecovered` increases

### Requirement: Failure reporting
A heap refusal before any flash write SHALL return HTTP 503, and authenticated status SHALL report whether the inactive slot was written. Uploads SHALL tolerate up to three consecutive 2 s receive timeouts within the transfer window.

#### Scenario: flash-begin refusal
- **WHEN** free heap is below 49,152 B at flash-begin
- **THEN** the upload fails with 503 and status reports `inactiveWritten: false`

### Requirement: Host headroom gate
The deployment CLI SHALL refuse to upload unless the authenticated session heap is at least 60,456 B, the measured upload demand above both device floors plus 3 KiB.

#### Scenario: Landscape source
- **WHEN** the session reports 55,044 B
- **THEN** the CLI stops with `memory_headroom` before any upload
