# Spec 01: Scoring State Machine

Status: APPROVED · Owner: Dale · Consumers: ESP32 firmware, Android app

## 1. Purpose

The ESP32 owns scoring. Android renders its authoritative BLE snapshot; the Kotlin
scoring mirror is a test oracle, not part of control flow. Doubles follows
traditional side-out scoring in the [2026 USA Pickleball Official Rulebook](https://usapickleball.org/docs/rules/USAP-Official-Rulebook.pdf),
§§4.A, 5.B, 5.B.4–6 and 6.B.2. This supersedes the former house rule. The board
does not track individual players or their positions within a team's end.

## 2. State

| Field | Meaning |
|---|---|
| `leftScore`, `rightScore` | Nonnegative integer points for the teams at the physical ends |
| `servingSide` | Physical end occupied by the current serving team |
| `serverNumber` | Doubles partner designation 1 or 2; singles BLE compatibility value 2 (not displayed) |
| `gameMode` | `DOUBLES` or `SINGLES` |
| `gameEnded` | Manually frozen by End Game |
| `openingService` | Internal: true only during the opening doubles service rotation; not sent over BLE |

## 3. Inputs (buttons)

- `RALLY_WON_LEFT`, `RALLY_WON_RIGHT`: which side won the rally. A serve fault
  is a loss for the serving side, not another attempt for the same player.
- `UNDO`, `SWITCH_COURTS`, `END_GAME`, `RESET`: see §5. Reset selects the
  starting team by its physical end and selects mode.

## 4. Initial state (game start / after RESET)

Boot defaults to doubles with Left serving. Reset lets the operator choose.
Both scores are zero; `gameEnded=false`. Doubles starts at `0–0–2`: the opening
team has one server, designated server 2 (`openingService=true`). Singles
starts `0–0` by official score call; the wire retains value 2 for compatibility
and `openingService=false`. Fresh initialization has no Undo history.

## 5. Transition rules

Except Reset and Undo, inputs during End Game do nothing. While live:

| Case | Outcome |
|---|---|
| A: serving side wins | Add one point to that team; retain the serving end, partner and opening status. A saturated integer counter does not change state or Undo. |
| B1: receiving side wins against normal doubles server 1 | No points. Same team keeps service; their partner becomes server 2. |
| B2: receiving side wins against the opening server or normal doubles server 2 | No points. Side-out to the receiving team at server 1. Clear opening status; normal rotations repeat for the rest of this game. |
| Switch Courts | Swap the physical score fields and flip the serving end. Keep partner number, mode and opening status. |

Server numbers denote partners for that rotation, not two attempts at a serve.
A point does not end the opening sequence; neither does changing court ends.

### 5a. UNDO

Record every accepted transition, including End Game and Reset. Undo restores
its prior position (including opening status), then removes that entry. Repeat
until the original position, then further Undo is a no-op. A new action after
Undo continues from the restored state; there is no Redo. Malformed commands
and ignored inputs never add, remove, or replace history. A same-position Reset
is still undoable. Reconnecting does not clear the journal; reboot does.

There is no fixed action-count limit or eviction. If the board cannot safely
extend its journal within available RAM, refuse the new transition without
mutating the game or dropping history and report the refusal on the extended
state stream (Spec 02 §5). Undo remains available when history exists.

### 5b. END_GAME

Set `gameEnded=true` without changing scoring or opening fields. Rallies and
court switches are ignored until Reset or Undo; repeated End Game is ignored.

### 5c. RESET

Snapshot the old position and initialize the new mode and starting end per §4;
this works while ended and is undoable.

## 6. Portrait display (derived from state)

Read serving score, opponent score, the server digit in Doubles only, then an
arrow toward the serving side's physical end (Spec 03). Switch Courts flips the
arrow but keeps presentation score order and any visible server digit. A
side-out flips the arrow and reverses score order; a partner change only changes
the server digit. End Game keeps the same pixels.

## 7. Worked example (doubles, Left opens)

| Rally winner | Left score | Right score | Serving end | Server |
|---|---:|---:|---|---:|
| Start | 0 | 0 | L | 2 |
| L | 1 | 0 | L | 2 |
| R | 1 | 0 | R | 1 |
| R | 1 | 1 | R | 1 |
| L | 1 | 1 | R | 2 |
| L | 1 | 1 | L | 1 |
| R | 1 | 1 | L | 2 |
| R | 1 | 1 | R | 1 |

## 8. Out of scope

Rally scoring (§14.B of the rulebook), game/match point detection or automatic
end, timeouts, pregame selection procedure, individual positions/correct server
by name, singles even/odd service-court positions, and simultaneous courts.

## 9. Resolved decisions

Operator selects the opening team, Undo walks the session history, and the
opening exception applies once per doubles game. End Game is manual and undoable.

### 9a. SINGLES

Per rulebook §§4.A, 5.A and 6.B.1, only a serving player can earn points and every
serving loss is an immediate side-out without points. There is no partner or
opening-service exception. A serving point changes the service court by score
parity, not the serving player's physical end. Keep BLE value 2 for compatibility;
render only the two scores and one arrow.

### 9b. Selecting game mode

Mode and opening side are selected together during Reset; no standalone
mid-game mode change is supported.
