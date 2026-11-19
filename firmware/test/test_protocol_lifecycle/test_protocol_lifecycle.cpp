#include <unity.h>
#include <limits>
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
    return UNITY_END();
}
