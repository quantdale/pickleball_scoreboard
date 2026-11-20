#include <unity.h>
#include <array>
#include <limits>
#include <vector>
#include "ble_command_ingress.h"
#include "ble_sync_gate.h"
#include "state_payload.h"

void setUp() {}
void tearDown() {}

void test_no_initial_notification_before_subscription()
{
    BleSyncGate gate;
    TEST_ASSERT_FALSE(gate.canNotify());
    gate.connect();
    TEST_ASSERT_FALSE(gate.canNotify());
    TEST_ASSERT_FALSE(gate.takeSync());
    gate.subscription(true);
    TEST_ASSERT_TRUE(gate.canNotify());
    TEST_ASSERT_TRUE(gate.takeSync());
    TEST_ASSERT_FALSE(gate.takeSync());
}

void test_disconnect_and_resubscribe_require_new_sync()
{
    BleSyncGate gate;
    gate.connect(); gate.subscription(true); gate.disconnect();
    TEST_ASSERT_FALSE(gate.canNotify());
    TEST_ASSERT_FALSE(gate.takeSync());
    gate.connect();
    TEST_ASSERT_FALSE(gate.takeSync());
    gate.subscription(true);
    TEST_ASSERT_TRUE(gate.takeSync());
    gate.subscription(false);
    TEST_ASSERT_FALSE(gate.canNotify());
    gate.subscription(true);
    TEST_ASSERT_TRUE(gate.takeSync());
}

void test_six_field_serialization()
{
    GameState state;
    TEST_ASSERT_EQUAL_STRING("0,0,L,2,0,D", serializeStatePayload(state).c_str());
    state.leftScore = 99; state.rightScore = 25; state.servingSide = Side::RIGHT;
    state.serverNumber = 1; state.gameEnded = true; state.gameMode = GameMode::SINGLES;
    const auto payload = serializeStatePayload(state);
    TEST_ASSERT_EQUAL_STRING("99,25,R,1,1,S", payload.c_str());
    TEST_ASSERT_TRUE(payload.size() <= 20);
}

void test_end_reset_undo_round_trip()
{
    GameState state;
    handleBleCommand("L", state); handleBleCommand("L", state);
    TEST_ASSERT_TRUE(handleBleCommand("E", state));
    TEST_ASSERT_FALSE(handleBleCommand("R", state));
    TEST_ASSERT_EQUAL_STRING("2,0,L,2,1,D", serializeStatePayload(state).c_str());
    TEST_ASSERT_TRUE(handleBleCommand("0RS", state));
    TEST_ASSERT_EQUAL_STRING("0,0,R,2,0,S", serializeStatePayload(state).c_str());
    TEST_ASSERT_TRUE(handleBleCommand("U", state));
    TEST_ASSERT_EQUAL_STRING("2,0,L,2,1,D", serializeStatePayload(state).c_str());
    TEST_ASSERT_TRUE(handleBleCommand("U", state));
    TEST_ASSERT_EQUAL_STRING("2,0,L,2,0,D", serializeStatePayload(state).c_str());
    TEST_ASSERT_TRUE(handleBleCommand("U", state));
    TEST_ASSERT_TRUE(handleBleCommand("U", state));
    TEST_ASSERT_FALSE(handleBleCommand("U", state));
}

void test_shared_rotations_over_ble()
{
    GameState state;
#define STEP(command, wire, opening, changed) \
    TEST_ASSERT_EQUAL(changed, handleBleCommand(command, state)); \
    TEST_ASSERT_EQUAL_STRING(wire, serializeStatePayload(state).c_str()); \
    TEST_ASSERT_EQUAL(opening, state.openingService);
#include "../../../shared/scoring_fixtures/serving-rotation.inc"
#undef STEP
}

void test_rapid_rallies_match_independent_fault_count_and_reconnect()
{
    GameState state;
    Side serving = Side::LEFT;
    int remainingServers = 1;
    int left = 0, right = 0;
    uint32_t rng = 0x1121u;
    for (int i = 0; i < 4096; ++i) {
        rng = rng * 1664525u + 1013904223u;
        const bool fault = (rng >> 31) != 0;
        const Side winner = !fault ? serving : (serving == Side::LEFT ? Side::RIGHT : Side::LEFT);
        TEST_ASSERT_TRUE(handleBleCommand(winner == Side::LEFT ? "L" : "R", state));
        if (!fault) ++(serving == Side::LEFT ? left : right);
        else if (--remainingServers == 0) {
            serving = winner;
            remainingServers = 2;
        }
        TEST_ASSERT_EQUAL(left, state.leftScore);
        TEST_ASSERT_EQUAL(right, state.rightScore);
        TEST_ASSERT_TRUE(serving == state.servingSide);
        TEST_ASSERT_EQUAL(3 - remainingServers, state.serverNumber);
    }
    const auto before = serializeStatePayload(state);
    BleSyncGate gate;
    gate.connect(); gate.subscription(true); gate.disconnect(); gate.connect(); gate.subscription(true);
    TEST_ASSERT_TRUE(gate.takeSync());
    TEST_ASSERT_EQUAL_STRING(before.c_str(), serializeStatePayload(state).c_str());
}

void test_score_overflow_does_not_wrap_or_replace_undo()
{
    GameState state;
    state.leftScore = std::numeric_limits<int>::max() - 1;
    TEST_ASSERT_TRUE(handleBleCommand("L", state));
    TEST_ASSERT_FALSE(handleBleCommand("L", state));
    TEST_ASSERT_TRUE(handleBleCommand("U", state));
    TEST_ASSERT_EQUAL(std::numeric_limits<int>::max() - 1, state.leftScore);
}

void test_invalid_commands_do_not_change_state_or_undo()
{
    GameState state;
    handleBleCommand("L", state);
    for (int byte = 1; byte < 128; ++byte) {
        if (byte == 'L' || byte == 'R' || byte == 'U' || byte == 'C' || byte == 'E') continue;
        TEST_ASSERT_FALSE(handleBleCommand(std::string(1, static_cast<char>(byte)), state));
        TEST_ASSERT_EQUAL(1, state.leftScore);
    }
    TEST_ASSERT_TRUE(handleBleCommand("U", state));
    TEST_ASSERT_EQUAL(0, state.leftScore);
}

void test_extended_history_status_updates_even_without_visible_change()
{
    GameState state;
    TEST_ASSERT_EQUAL_STRING("0,0,L,2,0,D,0", serializeExtendedStatePayload(state).c_str());
    TEST_ASSERT_FALSE(handleBleCommand("0LD", state)); // same position, undo status changed
    TEST_ASSERT_EQUAL_STRING("0,0,L,2,0,D,1", serializeExtendedStatePayload(state).c_str());
    BleSyncGate legacy, extended;
    legacy.connect(); extended.connect(); extended.subscription(true);
    TEST_ASSERT_FALSE(legacy.takeSync());
    TEST_ASSERT_TRUE(extended.takeSync());
    legacy.disconnect(); extended.disconnect();
    legacy.connect(); extended.connect(); extended.subscription(true);
    TEST_ASSERT_TRUE(extended.takeSync());
    TEST_ASSERT_EQUAL_STRING("0,0,L,2,0,D,1", serializeExtendedStatePayload(state).c_str());
    TEST_ASSERT_FALSE(handleBleCommand("U", state));
    TEST_ASSERT_EQUAL_STRING("0,0,L,2,0,D,0", serializeExtendedStatePayload(state).c_str());
}

void test_undo_unwinds_thousands_of_mixed_commands_in_both_modes()
{
    for (const auto mode : {GameMode::SINGLES, GameMode::DOUBLES}) {
        GameState state;
        initGameState(state, Side::RIGHT, mode);
        std::vector<GamePosition> timeline;
        BleSyncGate gate;
        for (int i = 0; i < 600; ++i) {
            const char* commands[] = {"L", "R", "R", "C", "L", "E", i % 2 ? "0LS" : "0RD"};
            for (const auto command : commands) {
                timeline.push_back(state);
                TEST_ASSERT_TRUE(handleBleCommand(command, state));
            }
            TEST_ASSERT_FALSE(handleBleCommand("0LX", state));
            if (i % 11 == 0) {
                const auto before = serializeExtendedStatePayload(state);
                gate.disconnect(); gate.connect();
                TEST_ASSERT_FALSE(gate.takeSync());
                gate.subscription(true);
                TEST_ASSERT_TRUE(gate.takeSync());
                TEST_ASSERT_EQUAL_STRING(before.c_str(), serializeExtendedStatePayload(state).c_str());
            }
        }
        TEST_ASSERT_EQUAL_UINT(5400, state.historyBytesUsed());
        for (auto it = timeline.rbegin(); it != timeline.rend(); ++it) {
            TEST_ASSERT_TRUE(state.canUndo());
            TEST_ASSERT_TRUE(handleBleCommand("U", state));
            TEST_ASSERT_TRUE(state.sameVisibleState(*it));
            TEST_ASSERT_EQUAL(it->openingService, state.openingService);
        }
        TEST_ASSERT_FALSE(state.canUndo());
        TEST_ASSERT_FALSE(handleBleCommand("U", state));
        TEST_ASSERT_EQUAL(0, state.leftScore);
        TEST_ASSERT_EQUAL(0, state.rightScore);
        TEST_ASSERT_TRUE(state.servingSide == Side::RIGHT && state.gameMode == mode);
    }
}

void test_journal_exhaustion_preserves_history_then_reuses_bytes()
{
    GameState state;
    for (int cycle = 0; cycle < 3; ++cycle) {
        for (std::size_t i = 0; i < GameState::HISTORY_BYTES - 1; ++i) {
            TEST_ASSERT_TRUE(handleBleCommand("L", state));
        }
        const GamePosition before = state;
        TEST_ASSERT_FALSE(handleBleCommand("0RS", state));
        TEST_ASSERT_TRUE(state.sameVisibleState(before));
        TEST_ASSERT_EQUAL_UINT(GameState::HISTORY_BYTES - 1, state.historyBytesUsed());
        TEST_ASSERT_EQUAL_UINT(3, state.undoStatus());
        TEST_ASSERT_TRUE(handleBleCommand("C", state)); // one byte fits
        TEST_ASSERT_EQUAL_UINT(GameState::HISTORY_BYTES, state.historyBytesUsed());
        TEST_ASSERT_EQUAL_UINT(1, state.undoStatus());
        TEST_ASSERT_FALSE(handleBleCommand("E", state));
        TEST_ASSERT_FALSE(handleBleCommand("R", state));
        TEST_ASSERT_EQUAL_UINT(3, state.undoStatus());
        TEST_ASSERT_FALSE(state.gameEnded);
        TEST_ASSERT_TRUE(handleBleCommand("U", state));
        TEST_ASSERT_TRUE(state.sameVisibleState(before));
        TEST_ASSERT_EQUAL_UINT(1, state.undoStatus());
        for (int score = state.leftScore; score > 0; --score) {
            TEST_ASSERT_TRUE(handleBleCommand("U", state));
            TEST_ASSERT_EQUAL_INT(score - 1, state.leftScore);
        }
        TEST_ASSERT_EQUAL_STRING("0,0,L,2,0,D,0", serializeExtendedStatePayload(state).c_str());
    }
}

void test_short_and_wide_reset_fit_exactly_and_restore_scores_and_flags()
{
    for (const auto score : {255, 256, std::numeric_limits<int>::max()}) {
        GameState state;
        initGameState(state, Side::RIGHT, GameMode::SINGLES);
        const std::size_t resetBytes = score == 255 ? 3 : 1 + 2 * sizeof(int);
        for (std::size_t i = 0; i <= GameState::HISTORY_BYTES - resetBytes; ++i)
            TEST_ASSERT_TRUE(handleBleCommand("C", state));
        state.rightScore = score;
        state.leftScore = 37;
        const GamePosition before = state;
        TEST_ASSERT_FALSE(handleBleCommand("0LD", state)); // one byte short
        TEST_ASSERT_TRUE(state.sameVisibleState(before));
        TEST_ASSERT_EQUAL_UINT(3, state.undoStatus());
        TEST_ASSERT_TRUE(handleBleCommand("U", state)); // exact space after popping a Switch
        const GamePosition restored = state;
        TEST_ASSERT_TRUE(handleBleCommand("0LD", state));
        TEST_ASSERT_EQUAL_UINT(GameState::HISTORY_BYTES, state.historyBytesUsed());
        TEST_ASSERT_TRUE(handleBleCommand("U", state));
        TEST_ASSERT_TRUE(state.sameVisibleState(restored));
        TEST_ASSERT_EQUAL(restored.openingService, state.openingService);
    }
}

void test_reset_only_capacity_preserves_same_position_resets()
{
    GameState state;
    const auto resets = GameState::HISTORY_BYTES / 3;
    for (std::size_t i = 0; i < resets; ++i) TEST_ASSERT_FALSE(handleBleCommand("0LD", state));
    TEST_ASSERT_EQUAL_UINT(resets * 3, state.historyBytesUsed());
    TEST_ASSERT_EQUAL_UINT(1, state.undoStatus());
    TEST_ASSERT_FALSE(handleBleCommand("0LD", state));
    TEST_ASSERT_EQUAL_STRING("0,0,L,2,0,D,3", serializeExtendedStatePayload(state).c_str());
    for (std::size_t i = 0; i < resets; ++i) {
        TEST_ASSERT_TRUE(state.canUndo());
        TEST_ASSERT_FALSE(handleBleCommand("U", state));
        TEST_ASSERT_EQUAL_UINT((resets - i - 1) * 3, state.historyBytesUsed());
    }
    TEST_ASSERT_EQUAL_UINT(0, state.undoStatus());
}

struct FakeWrite {
    bool need_rsp;
    bool is_prep;
    uint16_t offset;
    std::size_t len;
    const char* value;
};

void test_transport_acks_only_queued_commands_and_preserves_fifo_across_disconnect()
{
    GameState state;
    BleSyncGate gate;
    gate.connect(); gate.subscription(true);
    std::array<ScoringWrite, SCORING_QUEUE_DEPTH> pending{};
    std::size_t accepted = 0;
    auto submit = [&](const FakeWrite& write, ScoringReply expected, bool shouldEnqueue) {
        const std::size_t before = accepted;
        unsigned replies = 0;
        receiveScoringWrite(write, [&](const ScoringWrite& command) {
            if (accepted == pending.size()) return false;
            pending[accepted++] = command;
            return true;
        }, [&](ScoringReply actual) {
            ++replies;
            TEST_ASSERT_TRUE(actual == expected);
            TEST_ASSERT_EQUAL_UINT(before + shouldEnqueue, accepted); // before ATT success
        });
        TEST_ASSERT_EQUAL_UINT(write.need_rsp ? 1 : 0, replies);
        TEST_ASSERT_EQUAL_UINT(before + shouldEnqueue, accepted);
    };
    submit({true, true, 0, 1, "L"}, ScoringReply::Unsupported, false);
    submit({true, false, 1, 1, "L"}, ScoringReply::InvalidOffset, false);
    submit({false, false, 0, 1, "L"}, ScoringReply::Success, false);
    submit({true, false, 0, 3, "0LX"}, ScoringReply::Success, false);
    submit({true, false, 0, 2, "LR"}, ScoringReply::Success, false);
    for (std::size_t i = 0; i < pending.size() - 1; ++i)
        submit({true, false, 0, 1, "L"}, ScoringReply::Success, true);
    submit({true, false, 0, 1, "C"}, ScoringReply::Success, true);
    submit({true, false, 0, 3, "0RS"}, ScoringReply::QueueFull, false);
    gate.disconnect();
    TEST_ASSERT_FALSE(gate.canNotify());
    for (const auto& command : pending)
        TEST_ASSERT_TRUE(handleBleCommand(command.bytes, command.length, state));
    gate.connect(); gate.subscription(true);
    TEST_ASSERT_TRUE(gate.takeSync());
    TEST_ASSERT_EQUAL_UINT(SCORING_QUEUE_DEPTH - 1, state.rightScore);
    TEST_ASSERT_EQUAL_UINT(SCORING_QUEUE_DEPTH, state.historyBytesUsed());
    TEST_ASSERT_TRUE(state.servingSide == Side::RIGHT && state.gameMode == GameMode::DOUBLES);
    TEST_ASSERT_TRUE(handleBleCommand("U", state));
    TEST_ASSERT_EQUAL_UINT(SCORING_QUEUE_DEPTH - 1, state.leftScore);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_no_initial_notification_before_subscription);
    RUN_TEST(test_disconnect_and_resubscribe_require_new_sync);
    RUN_TEST(test_six_field_serialization);
    RUN_TEST(test_end_reset_undo_round_trip);
    RUN_TEST(test_shared_rotations_over_ble);
    RUN_TEST(test_rapid_rallies_match_independent_fault_count_and_reconnect);
    RUN_TEST(test_score_overflow_does_not_wrap_or_replace_undo);
    RUN_TEST(test_invalid_commands_do_not_change_state_or_undo);
    RUN_TEST(test_extended_history_status_updates_even_without_visible_change);
    RUN_TEST(test_undo_unwinds_thousands_of_mixed_commands_in_both_modes);
    RUN_TEST(test_journal_exhaustion_preserves_history_then_reuses_bytes);
    RUN_TEST(test_short_and_wide_reset_fit_exactly_and_restore_scores_and_flags);
    RUN_TEST(test_reset_only_capacity_preserves_same_position_resets);
    RUN_TEST(test_transport_acks_only_queued_commands_and_preserves_fifo_across_disconnect);
    return UNITY_END();
}
