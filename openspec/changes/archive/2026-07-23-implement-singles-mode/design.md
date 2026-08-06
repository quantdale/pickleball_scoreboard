## Context

Spec 01 Section 9a and Spec 02 Section 4a were just approved to add SINGLES mode and a 3-byte reset command. Both firmware and Android currently implement only the DOUBLES B1/B2 split and the 2-byte reset. `GameMode`/`gameMode` already exist as a field on both platforms' state structs (firmware `GameState`, Android `ScoreboardState`) but are unused by the transition logic and not settable via any input — they were added structurally ahead of this change and default to `DOUBLES` everywhere.

This change is cross-cutting (firmware + Android, state machine + wire protocol + UI), touches an existing wire format in a breaking way, and has two points the brief explicitly flagged as needing a resolved decision before coding — hence a design doc.

## Goals / Non-Goals

**Goals:**
- Branch Case B (fault by serving side) on `gameMode` per Spec 01 Section 9a, on both platforms, identically.
- Move RESET to the 3-byte wire format per Spec 02 Section 4a, on both platforms, as a hard breaking change (no 2-byte fallback).
- Resolve whether `gameMode` belongs in the state Notify payload.
- Add a game-mode selector to the Android reset dialog.
- Cover all of the above with tests; remove/replace any test asserting the old 2-byte reset.

**Non-Goals:**
- Court-position tracking within SINGLES (Spec 01 Section 8 — confirmed out of scope).
- Match-point/win detection (unrelated to this change).
- Any display-rendering change beyond confirming SINGLES doesn't need one (Spec 03 was not updated — see Decision 3).

## Decisions

### 1. `handleReset`/`reset` take an explicit `GameMode`/`Side` pair; no mode-change outside RESET

Firmware: `handleReset(GameState& state, Side startingSide, GameMode mode)`. Android: `ScoreboardStateMachine.reset(startingSide: Side, mode: GameMode)`. `initGameState`/`init` gain the same parameter. This matches Spec 01 Section 9b: game mode is selected only as part of RESET, never as a standalone mid-game input.

**Alternative considered:** a separate `SET_MODE` input. Rejected — Spec 01 Section 9b explicitly says mode changes only happen via a full Reset.

### 2. Case B branches on `gameMode`, nothing else changes

In both `handleRallyWonBy`/`applyRally`, when the winning side is not the current server:
- `DOUBLES`: unchanged existing B1/B2 split (serverNumber 1→2 stays same side; serverNumber 2 → full side-out).
- `SINGLES`: always full side-out immediately (`servingSide` flips to winner, `serverNumber` resets to 2), regardless of the current `serverNumber` value.

Since every game starts at `serverNumber = 2` (Spec 01 Section 4) and every SINGLES side-out also lands back on `serverNumber = 2`, `serverNumber` never actually reaches 1 in SINGLES during normal play from a 0-0-2 start — consistent with the same observation Spec 01 already makes about DOUBLES in Section 5's closing note. No new state field is needed; the branch is purely `if (gameMode == SINGLES) { fullSideOut } else { existing B1/B2 logic }`.

**Alternative considered:** collapsing to "SINGLES is just DOUBLES with B1 skipped" via a shared helper parameterized on whether the split step exists. Adopted in spirit — implementation should still factor the "full side-out" behavior (currently only B2) into a small shared helper called both by DOUBLES' B2 branch and unconditionally by SINGLES, to avoid duplicating the "flip serve, reset to server 2" logic in two places.

### 3. Reset is a hard 3-byte cutover; a 2-byte reset is treated as malformed, not legacy

Per the proposal's **BREAKING** note and Spec 02 Section 4a/7: `'0'` followed by anything other than exactly `<L|R><D|S>` (missing byte 2, missing byte 3, or either byte invalid) is ignored entirely — same "ignore silently, no notify" handling as any other malformed command. No transitional dual-format support.

**Alternative considered:** accepting a 2-byte reset as "reset to DOUBLES" for backward compatibility. Rejected per explicit instruction — this is a deliberate breaking change and both sides are updated together in the same change, so there's no deployed party that needs the old format to keep working.

### 4. `gameMode` is added to the Notify payload now

Spec 02 Section 5 already anticipated this: *"`gameMode` ... is deliberately not included in this message yet, since singles isn't implemented ... Add it here when singles support is actually built."* That condition is now true. New format:

```
<leftScore>,<rightScore>,<servingSide>,<serverNumber>,<gameEnded>,<gameMode>
```

`gameMode` encoded as a single character, `'D'` or `'S'`, appended at the end (rather than inserted in the middle) to keep the existing fields' positions stable for anything already parsing the first five fields positionally.

This isn't treated as an open question — Spec 02 already told us what to do once singles existed. Flagging it here per the brief's request, but the decision itself follows directly from the spec text rather than being invented.

**Alternative considered:** leave `gameMode` out of Notify and have the app track it purely from the reset dialog's own selection. Rejected: the app is supposed to be a "dumb renderer" of whatever the ESP32 reports (Spec 02 Section 1), and the ESP32 is the sole source of truth for `gameMode` just like every other field — an app-side-only guess would drift on reconnect (Spec 02 Section 6, where a reconnecting app must sync from the ESP32's state alone, not from memory of what it last sent).

### 5. Display rendering (Spec 03) needs no change

Spec 03 defines rendering purely from `(leftScore, rightScore, servingSide, serverNumber, gameEnded)` and was not updated alongside Specs 01/02. There is no approved visual distinction between SINGLES and DOUBLES, and this change does not invent one. The Notify payload's new `gameMode` field (Decision 4) does not need to be consumed by either renderer — firmware's `display_render`/Android's Compose renderer keep ignoring it, matching Spec 03 Section 6's defined state tuple exactly.

This is called out explicitly per the brief's request rather than assumed silently: **if a visual distinction between SINGLES/DOUBLES is wanted later, that requires a Spec 03 change first.** No action taken here.

### 6. Firmware test helper signatures gain a `GameMode` parameter with a default

`assertState`/`initGameState` call sites in existing tests don't need to change if `initGameState`/`handleReset` mode parameter defaults to `GameMode::DOUBLES` (firmware) — this keeps all pre-existing DOUBLES test cases compiling unchanged, since this change must not touch already-passing DOUBLES coverage. New tests pass `GameMode::SINGLES` explicitly. Same approach on Android: `reset`/`init` get an optional `mode: GameMode = GameMode.DOUBLES` parameter so existing call sites compile unchanged.

**Alternative considered:** making `gameMode` a required parameter everywhere and updating all existing call sites. Rejected as unnecessary churn — a defaulted parameter is simpler and the existing tests are explicitly meant to keep testing DOUBLES behavior.

## Risks / Trade-offs

- **[Risk] Breaking wire format could strand a firmware/app pair mid-upgrade** (old app talking to new firmware or vice versa) → Mitigation: both are built and flashed/installed together as part of this change, same as any other firmware/app pairing in this project; no OTA or staged rollout exists to make this a real concern yet (Spec 02 Section 8).
- **[Risk] Duplicating the "full side-out" logic between DOUBLES' B2 case and SINGLES' unconditional case could drift** → Mitigation: factor it into one shared helper per Decision 2, called from both branches.
- **[Risk] Appending `gameMode` to the end of the Notify payload still changes the field count, which could break a naive parser that hardcodes "5 fields"** → Mitigation: this is exactly why it's called out as breaking in the proposal; `parseStatePayload` and the firmware notify formatter are updated together in this change, and the field-count check in the Android parser is updated from `!= 5` to `!= 6`.

## Open Questions

None — the two ambiguities flagged in the brief (Notify payload field, display distinction) are resolved in Decisions 4 and 5 above rather than left open, since the specs themselves (Spec 02 Section 5's forward note, Spec 03's unchanged scope) already answer them.
