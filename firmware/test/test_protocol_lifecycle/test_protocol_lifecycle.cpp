#include <unity.h>
#include <limits>
#include <vector>
#include "ble_command_parser.h"
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
        for (int i = 0; i < 600; ++i) {
            const char* commands[] = {"L", "R", "R", "C", "L", "E", i % 2 ? "0LS" : "0RD"};
            for (const auto command : commands) {
                timeline.push_back(state);
                TEST_ASSERT_TRUE(handleBleCommand(command, state));
            }
            TEST_ASSERT_FALSE(handleBleCommand("0LX", state));
        }
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

static int remainingBlocks;
static bool admitBlock(std::size_t) { return remainingBlocks > 0 && --remainingBlocks >= 0; }

void test_history_allocation_failure_never_discards_or_partially_applies()
{
    remainingBlocks = 0;
    GameState state(admitBlock);
    TEST_ASSERT_FALSE(handleBleCommand("L", state));
    TEST_ASSERT_EQUAL_STRING("0,0,L,2,0,D,2", serializeExtendedStatePayload(state).c_str());
    remainingBlocks = 1;
    for (int i = 0; i < 511; ++i) TEST_ASSERT_TRUE(handleBleCommand("L", state));
    // A Reset needs more than the single remaining byte in the block.
    TEST_ASSERT_FALSE(handleBleCommand("0RS", state));
    TEST_ASSERT_EQUAL_STRING("511,0,L,2,0,D,3", serializeExtendedStatePayload(state).c_str());
    TEST_ASSERT_TRUE(handleBleCommand("C", state)); // fits without allocating again
    TEST_ASSERT_FALSE(handleBleCommand("E", state)); // now completely full
    TEST_ASSERT_TRUE(handleBleCommand("U", state));
    TEST_ASSERT_EQUAL_STRING("511,0,L,2,0,D,1", serializeExtendedStatePayload(state).c_str());
    for (int i = 0; i < 511; ++i) TEST_ASSERT_TRUE(handleBleCommand("U", state));
    TEST_ASSERT_EQUAL_STRING("0,0,L,2,0,D,0", serializeExtendedStatePayload(state).c_str());
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
    RUN_TEST(test_history_allocation_failure_never_discards_or_partially_applies);
    return UNITY_END();
}
