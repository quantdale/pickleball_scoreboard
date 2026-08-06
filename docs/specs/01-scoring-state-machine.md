# Spec 01: Scoring State Machine

Status: APPROVED
Owner: Dale
Consumers: ESP32 firmware, Android app

## 1. Purpose

Defines the exact, unambiguous rules for how the scoreboard's game state changes
in response to user input. This is the single source of truth for scoring logic.
Both the ESP32 firmware and the Android app must implement this spec faithfully.
Neither implementation is the "reference" — this document is.

## 2. State

The full game state at any moment is:

| Field | Type | Meaning |
|---|---|---|
| `leftScore` | integer, 0+ | Current score, left side |
| `rightScore` | integer, 0+ | Current score, right side |
| `servingSide` | L or R | Which side currently has serve |
| `serverNumber` | 1 or 2 | First or second server of the current side-out |
| `gameMode` | SINGLES or DOUBLES | Reserved for future use — see Section 9a |
| `gameEnded` | boolean | True after End Game pressed; freezes input handling |

Note: an earlier draft of this spec included an `isFirstServiceSequence` flag
to model the official "first serving team only gets one server" rule at the
very start of a game. This has been REMOVED per user decision: this project
does not model the coin-toss / serve-or-receive / play-for-serve sequence at
all. Instead, every game simply starts by asking the user which side is
"0-0-2" (see Section 4). This makes the first side-out behave exactly like
every other side-out — no special case needed.

## 3. Inputs (buttons)

| Input | Meaning |
|---|---|
| `RALLY_WON_LEFT` | Left side won the rally just played |
| `RALLY_WON_RIGHT` | Right side won the rally just played |
| `UNDO` | Revert to the single previous state (one step only, see Section 5a) |
| `SWITCH_COURTS` | Manually swap left/right (mid-game court switch) |
| `END_GAME` | Freeze the board — display holds final score, rally/switch inputs ignored until Reset; UNDO can still un-freeze (see Sections 5a/5b) |
| `RESET` | New game — re-prompt for 0-0-2 side, all fields to initial state |

Note: there is no separate "fault" button. A rally is always won by one side
or the other — if the serving side "faults" (e.g. serve into the net), that
is scored as the *other* side winning the rally. `RALLY_WON_LEFT` /
`RALLY_WON_RIGHT` are the only two rally-outcome inputs.

## 4. Initial state (game start / after RESET)

There is no coin-toss / serve-or-receive / play-for-serve modeling in this
app. Instead, at the start of every game (and after Reset), the app asks the
user directly: **"Which side is 0-0-2?"** — i.e. which side is about to serve,
starting as the second server. This single selector answers what would
otherwise require modeling the full pre-game sequence, because by house
convention play always begins at 0-0-2 regardless of how the sides got there.

```
leftScore = 0
rightScore = 0
servingSide = <user selection: L or R>
serverNumber = 2
gameMode = <user selection: DOUBLES or SINGLES>   (default DOUBLES; see Section 9a/9b)
gameEnded = false
```

## 5. Transition rules

All rules below apply only when `gameEnded == false`. If `gameEnded == true`,
every input except `RESET` and `UNDO` is ignored entirely (see Sections 5a
and 5b).

In DOUBLES mode (the current default — see Section 9a for SINGLES):

On `RALLY_WON_<SIDE>` where `<SIDE>` is L or R:

**Case A: the winning side IS the current server (`SIDE == servingSide`)**
→ That side's score increments by 1.
→ `servingSide` and `serverNumber` stay unchanged.
→ (Serving side won their own rally — they keep serving, keep score.)

**Case B: the winning side is NOT the current server (`SIDE != servingSide`)**
→ No score changes for anyone.
→ This is a fault/side-out event for the current server. Sub-cases:

  **B1: `serverNumber == 1`**
  → Same side keeps serving, but moves to their second server.
  → `servingSide` unchanged.
  → `serverNumber` becomes 2.

  **B2: `serverNumber == 2`**
  → Full side-out: serve passes to the winning side.
  → `servingSide` flips to the winning side.
  → `serverNumber` resets to 2 (per Section 4 — every side-out in this app
    starts the new server at 0-0-2 convention, i.e. `serverNumber = 2`, not 1).

Note: because games always start at second-server per Section 4's 0-0-2
convention, and every side-out in Case B2 also hands off directly to
`serverNumber = 2`, `serverNumber` in DOUBLES mode is effectively always 2 in
this implementation. `serverNumber = 1` only appears transiently after a
single fault, per Case B1, before either scoring (Case A) or faulting again
(Case B2). This is intentional — see Section 9a for how SINGLES mode would
differ.

On `SWITCH_COURTS`:
→ `leftScore` and `rightScore` swap values.
→ `servingSide` flips (L↔R) — because the side that was serving is now
  physically sitting on the other half of the court.
→ `serverNumber` unchanged.

## 5a. UNDO

Single-step only. The app/firmware keeps exactly one saved copy of the state:
"the state immediately before the last button press." Pressing `UNDO`
restores that saved copy and then clears it (so pressing `UNDO` twice in a
row does nothing the second time — there is no redo-of-undo).
Implementation note: save the previous state as a plain copy of the whole
state struct before applying any transition, rather than trying to
mathematically reverse a side-out — side-outs are not cleanly invertible
(you cannot tell from state B2's output alone whether Case A or Case B2 led
there).

`UNDO` itself does not save any "previous state" — you cannot undo an undo.

**UNDO applies uniformly to every input, including `END_GAME` and
`RESET`.** There is no special-case exclusion: `END_GAME` and `RESET` are
button presses like any other, so pressing `UNDO` immediately after either
one reverts to the state that existed immediately before it — i.e. UNDO can
un-freeze a just-ended game, and UNDO can restore the state that existed
immediately before an accidental `RESET`. This is a deliberate decision
(not just the "literal" reading of the rule above) — an accidental tap of
either button is exactly the kind of mistake UNDO exists to correct.

## 5b. END_GAME

→ Sets `gameEnded = true`. No score/serve fields change.
→ While `gameEnded == true`: `RALLY_WON_LEFT`, `RALLY_WON_RIGHT`, and
  `SWITCH_COURTS` are all no-ops. Display continues showing the frozen final
  score and server arrows exactly as they were at the moment `END_GAME` was
  pressed.
→ `UNDO` is NOT frozen: per Section 5a it applies uniformly to every input,
  so pressing `UNDO` immediately after `END_GAME` un-freezes the game and
  restores the live state that existed just before `END_GAME` was pressed
  (`gameEnded` returns to false, and normal input handling resumes).
→ Aside from `UNDO`, only `RESET` has any effect while `gameEnded == true`.

## 5c. RESET

→ Prompts the user again for "Which side is 0-0-2?" (Section 4), then sets
  state to the Section 4 initial state using that answer.
→ Works regardless of the current value of `gameEnded` — apart from `UNDO`
  (Section 5a/5b), this is the only input that has any effect on a
  frozen/ended board.

## 6. Display arrow rendering (derived from state, not stored separately)

- If `servingSide == L`: show serve arrow(s) on the left side.
- If `servingSide == R`: show serve arrow(s) on the right side.
- If `serverNumber == 1`: show one arrow.
- If `serverNumber == 2`: show two arrows.
- If `gameEnded == true`: arrows and score simply hold their last values —
  no special "game over" rendering is required for v1 (e.g. no blinking, no
  "GAME OVER" text). [Flag if you want something more visible later.]

## 7. Worked examples

Starting state after user selects "Left is 0-0-2": `(0, 0, L, 2)`
(fields shown: leftScore, rightScore, servingSide, serverNumber — gameMode
and gameEnded omitted from this table for brevity, assumed DOUBLES/false)

| # | Input | State before | New state | Why |
|---|---|---|---|---|
| 1 | `RALLY_WON_LEFT` | `(0,0,L,2)` | `(1,0,L,2)` | L serving, L won → score up |
| 2 | `RALLY_WON_RIGHT` | `(1,0,L,2)` | `(1,0,R,2)` | L serving (2nd), R won → side-out, serve to R at server 2 |
| 3 | `RALLY_WON_LEFT` | `(1,0,R,2)` | `(1,0,L,2)` | R serving (2nd), L won → side-out, serve to L at server 2 |

Note: because `serverNumber` starts at 2 and every side-out hands off
directly to server 2 again, in ordinary DOUBLES play under this spec the
"second server" concept never actually surfaces as a distinct visual state
during normal play from a 0-0-2 start — server is always shown as 2. This is
a direct consequence of skipping the first-service modeling per your
decision in Section 4, and matches the simplification you asked for.
[FLAG: if this feels wrong once you're testing on the court — e.g. you
expect to sometimes see a single-arrow "first server" state mid-game — raise
it, since it would mean Case B1 needs re-examining.]

## 8. Explicitly out of scope for v1

- Game/match point win conditions (playing to 11, win by 2, best of 3, etc.) —
  not handled. Scoreboard just counts; players/crowd decide when the game
  ends. Confirmed out of scope — no match-point indication of any kind.
- Timeouts.
- Multiple simultaneous scoreboards / multi-court.
- Court-position tracking within SINGLES mode (serve from right court on
  even score, left on odd) — SINGLES mode itself IS implemented (see
  Section 9a), but this specific display detail is not.

## 9. Resolved decisions (formerly open questions)

1. **Game start**: no coin-toss modeling; user selects which side is 0-0-2
   at game start and after Reset. See Section 4.
2. **Undo depth**: single-step only. See Section 5a.
3. **First-service single-server rule**: not implemented — superseded by
   decision 1 (starting directly at 0-0-2 makes it moot).
4. **Match point display**: not implemented, and not planned — the board
   only ever shows the current score.
5. **End Game button**: added. Freezes display, ignores further scoring
   input until Reset. See Section 5b.

## 9a. SINGLES mode — now defined

`gameMode` selects between `DOUBLES` (default) and `SINGLES`. The only
behavioral difference is in Section 5's Case B (fault by the serving
side): SINGLES has no second-server step at all.

**SINGLES transition rule for Case B (fault by serving side):**
→ Full side-out immediately. There is no B1/B2 split — a single fault
  always hands the serve directly to the winning side.
→ `servingSide` flips to the winning side.
→ `serverNumber` resets to 2, per the same 0-0-2-start convention used
  in DOUBLES (Section 4/5).
→ No score change (same as DOUBLES Case B — the fault itself never
  scores a point for anyone).

Everything else — Case A (server wins own rally → score increments),
`SWITCH_COURTS`, `UNDO`, `END_GAME`, `RESET` — behaves identically to
DOUBLES. `serverNumber` in SINGLES therefore behaves exactly like it does
in DOUBLES (effectively always 2 in practice, per the note in Section 5) —
the only change is that DOUBLES' two-fault Case B1/B2 split collapses
into a single always-side-out case for SINGLES.

**Explicitly out of scope for this mode (confirmed, not deferred):**
- Court-position tracking (official singles rules serve from the right
  court on even scores, left on odd) — not tracked or displayed. The
  scoreboard shows score and serving side only, same as DOUBLES.
- Any other singles-specific display element.

## 9b. Selecting game mode

Game mode is selected at the same time as the 0-0-2 side, per Section 4 —
i.e. as part of RESET, not as a separate standalone input. There is no way
to change `gameMode` mid-game without a full Reset.
