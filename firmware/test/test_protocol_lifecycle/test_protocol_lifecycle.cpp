#include <unity.h>
#include <array>
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

void test_legacy_and_manual_serialization_fit_default_mtu()
{
    GameState state;
    TEST_ASSERT_EQUAL_STRING("0,0,L,1,0,D", serializeStatePayload(state).c_str());
    TEST_ASSERT_EQUAL_STRING("0,0,L,1,0,D,0", serializeManualStatePayload(state).c_str());
    for (int i = 0; i < 120; ++i) { handleBleCommand("+L", state); handleBleCommand("+R", state); }
    handleBleCommand("S", state); handleBleCommand("S", state); handleBleCommand("S", state);
    handleBleCommand("E", state);
    const auto longest = serializeManualStatePayload(state);
    TEST_ASSERT_EQUAL_STRING("99,99,R,2,1,D,1", longest.c_str());
    TEST_ASSERT_TRUE(longest.size() <= 20); // Spec 02 §5: default ATT MTU Notify value
}

void test_shared_manual_controller_fixture_over_ble()
{
    GameState state;
#define STEP(command, wire, undo, changed) \
    TEST_ASSERT_EQUAL(changed, handleBleCommand(command, state)); \
    TEST_ASSERT_EQUAL_STRING(wire, serializeStatePayload(state).c_str()); \
    TEST_ASSERT_EQUAL_UINT(undo, state.undoStatus());
#include "../../../shared/scoring_fixtures/manual-controller.inc"
#undef STEP
}

void test_shared_random_walk_fixture_over_ble()
{
    GameState state;
#define STEP(command, wire, undo, changed) \
    TEST_ASSERT_EQUAL(changed, handleBleCommand(command, state)); \
    TEST_ASSERT_EQUAL_STRING(wire, serializeStatePayload(state).c_str()); \
    TEST_ASSERT_EQUAL_UINT(undo, state.undoStatus());
#include "../../../shared/scoring_fixtures/manual-random-walk.inc"
#undef STEP
}

void test_rapid_adjustments_match_independent_model_across_reconnects()
{
    GameState state;
    int left = 0, right = 0, step = 0; // step: 0=L1 1=L2 2=R1 3=R2
    uint32_t rng = 0x1121u;
    BleSyncGate gate;
    gate.connect(); gate.subscription(true);
    for (int i = 0; i < 6000; ++i) {
        rng = rng * 1664525u + 1013904223u;
        switch ((rng >> 28) % 5) {
            case 0: handleBleCommand("+L", state); left = left < 99 ? left + 1 : 99; break;
            case 1: handleBleCommand("+R", state); right = right < 99 ? right + 1 : 99; break;
            case 2: handleBleCommand("-L", state); left = left > 0 ? left - 1 : 0; break;
            case 3: handleBleCommand("-R", state); right = right > 0 ? right - 1 : 0; break;
            default: handleBleCommand("S", state); step = (step + 1) % 4; break;
        }
        TEST_ASSERT_EQUAL(left, state.leftScore);
        TEST_ASSERT_EQUAL(right, state.rightScore);
        TEST_ASSERT_TRUE((step < 2 ? Side::LEFT : Side::RIGHT) == state.servingSide);
        TEST_ASSERT_EQUAL(step % 2 + 1, state.serverNumber);
        if (i % 500 == 0) {
            const auto before = serializeManualStatePayload(state);
            gate.disconnect(); gate.connect();
            TEST_ASSERT_FALSE(gate.takeSync());
            gate.subscription(true);
            TEST_ASSERT_TRUE(gate.takeSync());
            TEST_ASSERT_EQUAL_STRING(before.c_str(), serializeManualStatePayload(state).c_str());
        }
    }
}

void test_invalid_commands_do_not_change_state_or_history()
{
    GameState state;
    handleBleCommand("+L", state);
    for (int byte = 1; byte < 128; ++byte) {
        if (byte == 'S' || byte == 'C' || byte == 'E' || byte == 'U') continue;
        TEST_ASSERT_FALSE(handleBleCommand(std::string(1, static_cast<char>(byte)), state));
        TEST_ASSERT_EQUAL(1, state.leftScore);
    }
    TEST_ASSERT_EQUAL_UINT(1, state.historyDepth());
    TEST_ASSERT_TRUE(handleBleCommand("U", state));
    TEST_ASSERT_EQUAL(0, state.leftScore);
}

void test_manual_undo_status_updates_without_visible_change()
{
    GameState state;
    TEST_ASSERT_FALSE(handleBleCommand("0D", state)); // same position, Undo became available
    TEST_ASSERT_EQUAL_STRING("0,0,L,1,0,D,1", serializeManualStatePayload(state).c_str());
    TEST_ASSERT_FALSE(handleBleCommand("U", state));
    TEST_ASSERT_EQUAL_STRING("0,0,L,1,0,D,0", serializeManualStatePayload(state).c_str());
}

void test_end_reset_undo_round_trip()
{
    GameState state;
    handleBleCommand("+L", state); handleBleCommand("+L", state);
    TEST_ASSERT_TRUE(handleBleCommand("E", state));
    TEST_ASSERT_FALSE(handleBleCommand("+R", state));
    TEST_ASSERT_EQUAL_STRING("2,0,L,1,1,D", serializeStatePayload(state).c_str());
    TEST_ASSERT_TRUE(handleBleCommand("0S", state));
    TEST_ASSERT_EQUAL_STRING("0,0,L,1,0,S", serializeStatePayload(state).c_str());
    TEST_ASSERT_TRUE(handleBleCommand("U", state));
    TEST_ASSERT_EQUAL_STRING("2,0,L,1,1,D", serializeStatePayload(state).c_str());
    TEST_ASSERT_TRUE(handleBleCommand("U", state));
    TEST_ASSERT_EQUAL_STRING("2,0,L,1,0,D", serializeStatePayload(state).c_str());
    TEST_ASSERT_TRUE(handleBleCommand("U", state));
    TEST_ASSERT_TRUE(handleBleCommand("U", state));
    TEST_ASSERT_FALSE(handleBleCommand("U", state));
    TEST_ASSERT_EQUAL_STRING("0,0,L,1,0,D,0", serializeManualStatePayload(state).c_str());
}

void test_long_session_keeps_accepting_and_unwinds_retained_history()
{
    for (const auto mode : {GameMode::SINGLES, GameMode::DOUBLES}) {
        GameState state;
        initGameState(state, mode);
        std::vector<GamePosition> timeline;
        for (int i = 0; i < 700; ++i) {
            const char* commands[] = {"+L", "+R", "S", "C", "-L", "E", i % 2 ? "0S" : "0D"};
            for (const auto command : commands) {
                timeline.push_back(state);
                const auto depth = state.historyDepth();
                handleBleCommand(command, state);
                TEST_ASSERT_TRUE(state.historyDepth() == depth + 1 || depth == GameState::HISTORY_POSITIONS);
            }
        }
        TEST_ASSERT_EQUAL_UINT(GameState::HISTORY_POSITIONS, state.historyDepth());
        for (std::size_t i = 0; i < GameState::HISTORY_POSITIONS; ++i) {
            handleBleCommand("U", state);
            TEST_ASSERT_TRUE(state.sameVisibleState(timeline[timeline.size() - 1 - i]));
        }
        TEST_ASSERT_FALSE(state.canUndo());
        TEST_ASSERT_FALSE(handleBleCommand("U", state));
    }
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
    submit({true, true, 0, 2, "+L"}, ScoringReply::Unsupported, false);
    submit({true, false, 1, 2, "+L"}, ScoringReply::InvalidOffset, false);
    submit({false, false, 0, 2, "+L"}, ScoringReply::Success, false);
    submit({true, false, 0, 3, "0LD"}, ScoringReply::Success, false); // portrait reset: ignored
    submit({true, false, 0, 1, "L"}, ScoringReply::Success, false);   // portrait rally: ignored
    for (std::size_t i = 0; i < pending.size() - 1; ++i)
        submit({true, false, 0, 2, "+R"}, ScoringReply::Success, true);
    submit({true, false, 0, 1, "S"}, ScoringReply::Success, true);
    submit({true, false, 0, 2, "0S"}, ScoringReply::QueueFull, false);
    gate.disconnect();
    TEST_ASSERT_FALSE(gate.canNotify());
    for (const auto& command : pending)
        TEST_ASSERT_TRUE(handleBleCommand(command.bytes, command.length, state));
    gate.connect(); gate.subscription(true);
    TEST_ASSERT_TRUE(gate.takeSync());
    TEST_ASSERT_EQUAL_STRING("0,15,L,2,0,D,1", serializeManualStatePayload(state).c_str());
    TEST_ASSERT_TRUE(handleBleCommand("U", state));
    TEST_ASSERT_EQUAL_STRING("0,15,L,1,0,D,1", serializeManualStatePayload(state).c_str());
}

// Spec 02 §§4–6: display settings ride the same acknowledged FIFO, behind earlier scoring.
void test_setting_writes_share_the_fifo_and_survive_reconnect()
{
    GameState state;
    DisplaySettings settings;
    BleSyncGate gate;
    std::vector<ScoringWrite> queue;
    auto submit = [&](const char* value) {
        unsigned replies = 0;
        receiveScoringWrite(FakeWrite{true, false, 0, std::string(value).size(), value},
            [&](const ScoringWrite& command) { queue.push_back(command); return true; },
            [&](ScoringReply reply) { ++replies; TEST_ASSERT_TRUE(reply == ScoringReply::Success); });
        TEST_ASSERT_EQUAL_UINT(1, replies);
    };
    gate.connect(); gate.subscription(true);
    for (const char* value : {"+L", "B3", "KR", "B9", "B0", "KX", "S", "B2", "KG", "+R"}) submit(value);
    TEST_ASSERT_EQUAL_UINT(7, queue.size()); // B9, B0 and KX acknowledged, never queued
    const char* expected[] = {"+L", "B3", "KR", "S", "B2", "KG", "+R"};
    for (std::size_t i = 0; i < queue.size(); ++i)
        TEST_ASSERT_EQUAL_STRING_LEN(expected[i], queue[i].bytes, queue[i].length);
    unsigned brightnessReports = 0, colorReports = 0;
    for (const auto& command : queue) {
        const auto effect = applyBoardCommand(command.bytes, command.length, state, settings, 0);
        brightnessReports += effect.brightnessChanged;
        colorReports += effect.colorChanged;
    }
    TEST_ASSERT_EQUAL_UINT(2, brightnessReports);
    TEST_ASSERT_EQUAL_UINT(2, colorReports);
    TEST_ASSERT_EQUAL_UINT8(2, settings.level());
    TEST_ASSERT_TRUE(settings.color() == LedColor::Green);
    TEST_ASSERT_EQUAL_STRING("1,1,L,2,0,D,1", serializeManualStatePayload(state).c_str());
    gate.disconnect(); gate.connect(); gate.subscription(true);
    TEST_ASSERT_TRUE(gate.takeSync()); // resync reports the retained settings, unchanged
    TEST_ASSERT_EQUAL_STRING("B,2,4", serializeBrightnessPayload(settings.level()).c_str());
    TEST_ASSERT_EQUAL_STRING("K,G", serializeColorPayload(settings.color()).c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_no_initial_notification_before_subscription);
    RUN_TEST(test_disconnect_and_resubscribe_require_new_sync);
    RUN_TEST(test_legacy_and_manual_serialization_fit_default_mtu);
    RUN_TEST(test_shared_manual_controller_fixture_over_ble);
    RUN_TEST(test_shared_random_walk_fixture_over_ble);
    RUN_TEST(test_rapid_adjustments_match_independent_model_across_reconnects);
    RUN_TEST(test_invalid_commands_do_not_change_state_or_history);
    RUN_TEST(test_manual_undo_status_updates_without_visible_change);
    RUN_TEST(test_end_reset_undo_round_trip);
    RUN_TEST(test_long_session_keeps_accepting_and_unwinds_retained_history);
    RUN_TEST(test_transport_acks_only_queued_commands_and_preserves_fifo_across_disconnect);
    RUN_TEST(test_setting_writes_share_the_fifo_and_survive_reconnect);
    return UNITY_END();
}
