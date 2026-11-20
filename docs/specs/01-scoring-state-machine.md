# Spec 01: Manual scoring state

Status: APPROVED (landscape redesign) · Owner: Dale · Consumers: ESP32 firmware, Android app

## 1. Purpose

Courtside testing showed that rally-winner scoring was too hard to operate during
a match. The board now records what the operator tells it: each team's score is
adjusted directly and the serving indicator is set by hand. The ESP32 still owns
the authoritative state; Android renders the BLE snapshot and never computes it.
The Kotlin mirror in test sources is a test oracle only.

The portrait rally-scoring implementation is preserved at tag
`portrait-1.3.1-court-undo-003` (branch `archive/portrait-1.3.1-court-undo-003`).

## 2. State

| Field | Meaning |
|---|---|
| `leftScore`, `rightScore` | Points for the teams at the physical left and right ends, 0–99 |
| `servingSide` | Physical end of the serving team, `L` or `R` |
| `serverNumber` | 1 or 2 in Doubles; always 1 in Singles |
| `gameMode` | `DOUBLES` or `SINGLES` |
| `gameEnded` | Set by End Game; freezes play inputs |

## 3. Inputs

| Input | Effect while live |
|---|---|
| `SCORE_UP(side)` | Add one point to that end's score |
| `SCORE_DOWN(side)` | Remove one point from that end's score |
| `NEXT_SERVE` | Advance the serving indicator one step (§5) |
| `SWITCH_COURTS` | Teams change ends: swap the two scores and flip `servingSide`; keep `serverNumber` |
| `END_GAME` | Set `gameEnded` |
| `UNDO` | Restore the position before the last accepted input (§6) |
| `RESET(mode)` | Start a new game in the chosen mode (§4) |

Score inputs are corrections, not rally results: they never change the serving
indicator, and the serving indicator never changes a score.

A score already at 99 ignores `SCORE_UP`; a score at 0 ignores `SCORE_DOWN`.
While `gameEnded` is set, score, serve, switch and repeated End Game inputs are
ignored. Undo and Reset always work. Ignored inputs change nothing, are not
recorded and are not notified.

## 4. Initial state

Boot and every Reset produce `0–0`, Left serving, server 1, not ended. Boot is
Doubles; Reset selects Doubles or Singles. The operator sets a different first
server with `NEXT_SERVE`. Boot starts with empty history; Reset is recorded like
any other input, even when the position does not change.

## 5. Serving indicator

Doubles cycles through exactly four states, one step per `NEXT_SERVE`:

```text
L1 → L2 → R1 → R2 → L1 …
```

`L1` is one arrow pointing left; `L2` two arrows pointing left; `R1`/`R2` point
right. Singles has no partner designation and alternates `L1 → R1 → L1 …`.

## 6. Undo history

Every accepted input stores the complete previous position. Undo restores the
most recent one and removes it; with no history, Undo is ignored. There is no
Redo. A new input after Undo continues from the restored position. Undo while the
game is ended restores the live position before End Game, so it resumes play.

History lives in a fixed RAM journal of 2,048 positions, allocated statically;
scoring never allocates heap. When the journal is full the oldest position is
dropped. A score or serve correction is never refused for lack of history space,
because every visible field can also be corrected directly. Reconnecting keeps
history; reboot clears it.

## 7. Worked example (Doubles)

| Input | Left | Right | Serve | Ended | Undo available |
|---|---:|---:|---|---|---|
| Boot | 0 | 0 | L1 | no | no |
| Left + ×7 | 7 | 0 | L1 | no | yes |
| Right + ×5 | 7 | 5 | L1 | no | yes |
| Next serve | 7 | 5 | L2 | no | yes |
| Next serve | 7 | 5 | R1 | no | yes |
| Right − | 7 | 4 | R1 | no | yes |
| Switch courts | 4 | 7 | L1 | no | yes |
| End game | 4 | 7 | L1 | yes | yes |
| Left + (ignored) | 4 | 7 | L1 | yes | yes |
| Undo | 4 | 7 | L1 | no | yes |
| Reset Singles | 0 | 0 | L1 | no | yes |
| Next serve | 0 | 0 | R1 | no | yes |

## 8. Out of scope

Automatic rally/side-out scoring, rule enforcement, game or match point
detection, automatic game end, timeouts, player names and positions, and
multiple courts.
