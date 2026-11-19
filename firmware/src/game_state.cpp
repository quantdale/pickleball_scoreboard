// Game state machine implementation.
// See docs/specs/01-scoring-state-machine.md for the authoritative rules.

#include "game_state.h"
#include <cstring>
#include <limits>
#include <new>

// Spec 01 §5a: reversible updates need only the old flags; Reset also needs
// both scores. Records never straddle blocks, and no entry is evicted.
struct GameState::Block {
    Block* older;
    std::size_t used;
    uint8_t bytes[512];
};

GameState::~GameState() { clearHistory(); }

void GameState::clearHistory() {
    while (history) {
        Block* old = history;
        history = old->older;
        delete old;
    }
    blocked = false;
}

bool GameState::record(Change change) {
    constexpr std::size_t scoreBytes = 2 * sizeof(int);
    const std::size_t needed = change == Change::Reset ? scoreBytes + 1 : 1;
    if (!history || sizeof(history->bytes) - history->used < needed) {
        Block* next = !admitBlock || admitBlock(sizeof(Block)) ? new (std::nothrow) Block : nullptr;
        if (!next) {
            blocked = true;
            return false;
        }
        next->older = history;
        next->used = 0;
        history = next;
    }
    if (change == Change::Reset) {
        const int scores[] = {leftScore, rightScore};
        std::memcpy(history->bytes + history->used, scores, scoreBytes);
        history->used += scoreBytes;
    }
    history->bytes[history->used++] = static_cast<uint8_t>(change)
        | (servingSide == Side::RIGHT ? 4 : 0) | (serverNumber == 2 ? 8 : 0)
        | (gameMode == GameMode::SINGLES ? 16 : 0) | (gameEnded ? 32 : 0)
        | (openingService ? 64 : 0);
    blocked = false;
    return true;
}

static void swapScores(GamePosition& state) {
    const int left = state.leftScore;
    state.leftScore = state.rightScore;
    state.rightScore = left;
}

void GameState::undo() {
    if (!history) return;
    const uint8_t flags = history->bytes[--history->used];
    servingSide = flags & 4 ? Side::RIGHT : Side::LEFT;
    serverNumber = flags & 8 ? 2 : 1;
    gameMode = flags & 16 ? GameMode::SINGLES : GameMode::DOUBLES;
    gameEnded = (flags & 32) != 0;
    openingService = (flags & 64) != 0;
    switch (static_cast<Change>(flags & 3)) {
        case Change::Flags: break;
        case Change::Point: --(servingSide == Side::LEFT ? leftScore : rightScore); break;
        case Change::Switch: swapScores(*this); break;
        case Change::Reset: {
            int scores[2];
            history->used -= sizeof(scores);
            std::memcpy(scores, history->bytes + history->used, sizeof(scores));
            leftScore = scores[0];
            rightScore = scores[1];
            break;
        }
    }
    if (history->used == 0) {
        Block* old = history;
        history = old->older;
        delete old;
    }
    blocked = false;
}

static Side otherSide(Side side) {
    return (side == Side::LEFT) ? Side::RIGHT : Side::LEFT;
}

static GamePosition openingPosition(Side startingSide, GameMode mode) {
    GamePosition position;
    position.servingSide = startingSide;
    position.gameMode = mode;
    position.openingService = mode == GameMode::DOUBLES;
    return position;
}

void initGameState(GameState& state, Side startingSide, GameMode mode) {
    static_cast<GamePosition&>(state) = openingPosition(startingSide, mode);
    state.clearHistory();
}

static void handleRallyWonBy(GameState& state, Side winningSide) {
    if (state.gameEnded) {
        return;
    }

    if (winningSide == state.servingSide) {
        // Spec 01 §5 A: only the serving side scores; a saturated counter is a no-op.
        int& score = winningSide == Side::LEFT ? state.leftScore : state.rightScore;
        if (score == std::numeric_limits<int>::max()) return;
        if (state.record(GameState::Change::Point)) ++score;
        return;
    }

    if (!state.record(GameState::Change::Flags)) return;
    const bool doubles = state.gameMode == GameMode::DOUBLES;
    if (doubles && !state.openingService && state.serverNumber == 1) {
        // Spec 01 §5 B1: partner takes over on the same team; no point.
        state.serverNumber = 2;
        return;
    }
    // Spec 01 §5 B2 and §9a: after a side-out, normal doubles starts on 1.
    state.openingService = false;
    state.servingSide = winningSide;
    state.serverNumber = doubles ? 1 : 2;
}

void handleRallyWonLeft(GameState& state) {
    handleRallyWonBy(state, Side::LEFT);
}

void handleRallyWonRight(GameState& state) {
    handleRallyWonBy(state, Side::RIGHT);
}

void handleUndo(GameState& state) {
    state.undo(); // Spec 01 §5a: includes END_GAME and RESET.
}

void handleSwitchCourts(GameState& state) {
    if (state.gameEnded) {
        return;
    }

    if (!state.record(GameState::Change::Switch)) return;
    swapScores(state);
    state.servingSide = otherSide(state.servingSide);
}

void handleEndGame(GameState& state) {
    if (state.gameEnded) {
        return;
    }

    if (state.record(GameState::Change::Flags)) state.gameEnded = true;
}

void handleReset(GameState& state, Side startingSide, GameMode mode) {
    if (state.record(GameState::Change::Reset)) {
        static_cast<GamePosition&>(state) = openingPosition(startingSide, mode);
    }
}
