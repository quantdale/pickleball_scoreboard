## Context

USA Pickleball 2026 §§4.A, 5.A, 5.B, 6.B: only servers earn a point under standard scoring; the opening doubles server is designated second, normal receiving teams take over on server 1.

## Goals / Non-Goals

Restore official team/server transitions, isolated one-step Undo and a compatible six-field projection. Do not track individual physical service courts, implement rally scoring or change portrait geometry, BLE wire format, installed firmware or OTA trust.

## Decisions

- Make the opening doubles sequence explicit and snapshot it with the rest of the match. Switch Courts changes physical ends without advancing the sequence. Singles keeps its compatibility digit 2 and has no opening flag.
- Move firmware Undo into the match instance. Distinguish fresh initialization (empty history) from user Reset (undoable). Keep Kotlin's model test-only and the Android app a BLE-state renderer.
- Verify real command sequences and projections; audit adjacent synchronization, queue and release gates, hardening only demonstrated issues.

## Risks / Trade-offs

Transport acknowledgment is not an application acknowledgment; under an uncertain disconnect the app must resynchronize, never replay. Real RF, rollback execution and long-run resource behavior remain hardware gates. Preserve the installed image and fallback.
