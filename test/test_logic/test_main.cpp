// Host-native tests for the protocol parser and relay state machine.
// Run with: pio test -e native

#include <unity.h>

#include <vector>

#include "hismith_protocol.h"
#include "relay.h"

using hismith::Frame;
using hismith::Parser;
using hismith::RelayController;

void setUp() {}
void tearDown() {}

// --- helpers ----------------------------------------------------------------

namespace {

// Feeds bytes through the parser and collects every frame that comes out,
// mirroring how main.cpp drains the RX queue.
std::vector<Frame> feed(Parser& parser, const std::vector<uint8_t>& bytes) {
    std::vector<Frame> frames;
    for (uint8_t b : bytes) {
        Frame frame;
        if (parser.push(b, frame) == Parser::Result::Frame) {
            frames.push_back(frame);
        }
    }
    return frames;
}

RelayController::Config cfg(uint32_t hold = 300, uint32_t max_on = 30000, uint32_t cooldown = 2000) {
    return RelayController::Config{hold, max_on, cooldown};
}

}  // namespace

// --- protocol: checksum rule ------------------------------------------------

// The rule was derived from the documented samples rather than specified, so
// pin it against every published Hismith frame we have.
void test_checksum_matches_documented_frames() {
    TEST_ASSERT_EQUAL_HEX8(0x0c, hismith::checksum(0x0b, 0x01));  // Piupiu lube squirt
    TEST_ASSERT_EQUAL_HEX8(0x01, hismith::checksum(0x01, 0x00));  // S2 get setup
    TEST_ASSERT_EQUAL_HEX8(0xa2, hismith::checksum(0x01, 0xa1));  // S2 position mode
}

void test_checksum_wraps_at_eight_bits() {
    TEST_ASSERT_EQUAL_HEX8(0x00, hismith::checksum(0xff, 0x01));
    TEST_ASSERT_EQUAL_HEX8(0xfe, hismith::checksum(0xff, 0xff));
}

// --- protocol: parsing ------------------------------------------------------

void test_squirt_frame_parses() {
    Parser parser;
    auto frames = feed(parser, {0xcc, 0x0b, 0x01, 0x0c});
    TEST_ASSERT_EQUAL_UINT32(1, frames.size());
    TEST_ASSERT_TRUE(frames[0].isSquirt());
    TEST_ASSERT_EQUAL_UINT32(1, parser.framesParsed());
    TEST_ASSERT_EQUAL_UINT32(0, parser.badChecksums());
}

void test_bad_checksum_is_rejected() {
    Parser parser;
    auto frames = feed(parser, {0xcc, 0x0b, 0x01, 0xff});
    TEST_ASSERT_EQUAL_UINT32(0, frames.size());
    TEST_ASSERT_EQUAL_UINT32(1, parser.badChecksums());
}

void test_valid_frame_with_unknown_command_is_not_a_squirt() {
    Parser parser;
    auto frames = feed(parser, {0xcc, 0x09, 0x05, 0x0e});
    TEST_ASSERT_EQUAL_UINT32(1, frames.size());
    TEST_ASSERT_FALSE(frames[0].isSquirt());
    TEST_ASSERT_EQUAL_HEX8(0x09, frames[0].cmd);
    TEST_ASSERT_EQUAL_HEX8(0x05, frames[0].arg);
}

// The lube command with a different argument must not fire the pump.
void test_lube_command_with_other_arg_is_not_a_squirt() {
    Parser parser;
    auto frames = feed(parser, {0xcc, 0x0b, 0x02, 0x0d});
    TEST_ASSERT_EQUAL_UINT32(1, frames.size());
    TEST_ASSERT_FALSE(frames[0].isSquirt());
}

// ffe9 is a BLE-serial stream: one frame may arrive across several writes.
void test_frame_split_across_writes() {
    Parser parser;
    auto a = feed(parser, {0xcc, 0x0b});
    TEST_ASSERT_EQUAL_UINT32(0, a.size());
    auto b = feed(parser, {0x01});
    TEST_ASSERT_EQUAL_UINT32(0, b.size());
    auto c = feed(parser, {0x0c});
    TEST_ASSERT_EQUAL_UINT32(1, c.size());
    TEST_ASSERT_TRUE(c[0].isSquirt());
}

// ...and several frames may arrive in one write.
void test_concatenated_frames_in_one_write() {
    Parser parser;
    auto frames = feed(parser, {0xcc, 0x0b, 0x01, 0x0c, 0xcc, 0x0b, 0x01, 0x0c, 0xcc, 0x0b, 0x01, 0x0c});
    TEST_ASSERT_EQUAL_UINT32(3, frames.size());
    for (const auto& f : frames) {
        TEST_ASSERT_TRUE(f.isSquirt());
    }
}

void test_leading_noise_is_ignored() {
    Parser parser;
    auto frames = feed(parser, {0x00, 0xff, 0x42, 0xcc, 0x0b, 0x01, 0x0c});
    TEST_ASSERT_EQUAL_UINT32(1, frames.size());
    TEST_ASSERT_TRUE(frames[0].isSquirt());
}

void test_resyncs_after_a_corrupt_frame() {
    Parser parser;
    auto frames = feed(parser, {0xcc, 0xde, 0xad, 0xbe, 0xcc, 0x0b, 0x01, 0x0c});
    TEST_ASSERT_EQUAL_UINT32(1, frames.size());
    TEST_ASSERT_TRUE(frames[0].isSquirt());
    TEST_ASSERT_EQUAL_UINT32(1, parser.badChecksums());
}

// A real header can land inside a frame that fails its checksum. Dropping the
// whole buffer would swallow it, so resync scans what is already buffered.
void test_resync_recovers_header_inside_a_bad_frame() {
    Parser parser;
    auto frames = feed(parser, {0xcc, 0x00, 0xcc, 0x0b, 0x01, 0x0c});
    TEST_ASSERT_EQUAL_UINT32(1, frames.size());
    TEST_ASSERT_TRUE(frames[0].isSquirt());
}

void test_reset_drops_partial_frame() {
    Parser parser;
    feed(parser, {0xcc, 0x0b});
    parser.reset();
    auto frames = feed(parser, {0x01, 0x0c});
    TEST_ASSERT_EQUAL_UINT32(0, frames.size());
}

// --- relay: hold while repeating --------------------------------------------

void test_squirt_energises_relay() {
    RelayController relay(cfg());
    TEST_ASSERT_FALSE(relay.energised());
    relay.squirt(1000);
    TEST_ASSERT_TRUE(relay.energised());
}

void test_relay_releases_after_hold_expires() {
    RelayController relay(cfg(300));
    relay.squirt(1000);
    relay.tick(1299);
    TEST_ASSERT_TRUE(relay.energised());
    relay.tick(1300);
    TEST_ASSERT_FALSE(relay.energised());
    TEST_ASSERT_EQUAL(RelayController::State::Idle, relay.state());
}

// The core of "hold while repeating": a stream of commands keeps it closed.
void test_repeated_squirts_extend_the_hold() {
    RelayController relay(cfg(300));
    relay.squirt(1000);
    for (uint32_t t = 1200; t <= 3000; t += 200) {
        relay.tick(t);
        relay.squirt(t);
        TEST_ASSERT_TRUE(relay.energised());
    }
    relay.tick(3299);
    TEST_ASSERT_TRUE(relay.energised());
    relay.tick(3300);
    TEST_ASSERT_FALSE(relay.energised());
}

// --- relay: failsafes -------------------------------------------------------

void test_max_on_trips_lockout_even_while_commands_continue() {
    RelayController relay(cfg(300, 5000, 2000));
    relay.squirt(0);
    for (uint32_t t = 100; t < 5000; t += 100) {
        relay.tick(t);
        relay.squirt(t);
    }
    TEST_ASSERT_TRUE(relay.energised());
    relay.tick(5000);
    TEST_ASSERT_FALSE(relay.energised());
    TEST_ASSERT_EQUAL(RelayController::State::Lockout, relay.state());
    TEST_ASSERT_TRUE(relay.consumeLockoutEvent());
    TEST_ASSERT_FALSE(relay.consumeLockoutEvent());
}

void test_squirts_are_ignored_during_lockout() {
    RelayController relay(cfg(300, 5000, 2000));
    relay.squirt(0);
    relay.tick(5000);
    TEST_ASSERT_EQUAL(RelayController::State::Lockout, relay.state());

    relay.squirt(5100);
    relay.tick(5100);
    TEST_ASSERT_FALSE(relay.energised());
}

void test_lockout_expires_after_cooldown() {
    RelayController relay(cfg(300, 5000, 2000));
    relay.squirt(0);
    relay.tick(5000);
    relay.tick(6999);
    TEST_ASSERT_EQUAL(RelayController::State::Lockout, relay.state());
    relay.tick(7000);
    TEST_ASSERT_EQUAL(RelayController::State::Idle, relay.state());

    relay.squirt(7100);
    TEST_ASSERT_TRUE(relay.energised());
}

// After a lockout and recovery the max-on budget must start fresh, not resume
// mid-way and trip again immediately.
void test_max_on_budget_resets_after_lockout() {
    RelayController relay(cfg(300, 5000, 2000));
    relay.squirt(0);
    relay.tick(5000);  // trips lockout
    TEST_ASSERT_TRUE(relay.consumeLockoutEvent());
    relay.tick(7000);  // cooldown done, back to Idle

    // A fresh hold gets the full max-on budget again: hold continuously for
    // just under 5s and it must not trip a second time.
    relay.squirt(7100);
    for (uint32_t t = 7200; t < 12000; t += 100) {
        relay.tick(t);
        relay.squirt(t);
    }
    TEST_ASSERT_TRUE(relay.energised());
    TEST_ASSERT_EQUAL(RelayController::State::Holding, relay.state());
    TEST_ASSERT_FALSE(relay.consumeLockoutEvent());
}

void test_force_off_releases_immediately() {
    RelayController relay(cfg());
    relay.squirt(1000);
    TEST_ASSERT_TRUE(relay.energised());
    relay.forceOff(1010);
    TEST_ASSERT_FALSE(relay.energised());
    TEST_ASSERT_EQUAL(RelayController::State::Idle, relay.state());
}

void test_force_off_does_not_wedge_the_relay() {
    RelayController relay(cfg());
    relay.squirt(1000);
    relay.forceOff(1010);
    relay.squirt(1020);
    TEST_ASSERT_TRUE(relay.energised());
}

// millis() wraps every ~49.7 days. A naive `now > deadline` would latch the
// relay on across the wrap.
void test_hold_survives_millis_rollover() {
    RelayController relay(cfg(300));
    const uint32_t near_wrap = 0xFFFFFF00;
    relay.squirt(near_wrap);
    relay.tick(near_wrap + 100);  // still before wrap
    TEST_ASSERT_TRUE(relay.energised());
    relay.tick(0x2C);  // wrapped past the deadline
    TEST_ASSERT_FALSE(relay.energised());
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_checksum_matches_documented_frames);
    RUN_TEST(test_checksum_wraps_at_eight_bits);

    RUN_TEST(test_squirt_frame_parses);
    RUN_TEST(test_bad_checksum_is_rejected);
    RUN_TEST(test_valid_frame_with_unknown_command_is_not_a_squirt);
    RUN_TEST(test_lube_command_with_other_arg_is_not_a_squirt);
    RUN_TEST(test_frame_split_across_writes);
    RUN_TEST(test_concatenated_frames_in_one_write);
    RUN_TEST(test_leading_noise_is_ignored);
    RUN_TEST(test_resyncs_after_a_corrupt_frame);
    RUN_TEST(test_resync_recovers_header_inside_a_bad_frame);
    RUN_TEST(test_reset_drops_partial_frame);

    RUN_TEST(test_squirt_energises_relay);
    RUN_TEST(test_relay_releases_after_hold_expires);
    RUN_TEST(test_repeated_squirts_extend_the_hold);

    RUN_TEST(test_max_on_trips_lockout_even_while_commands_continue);
    RUN_TEST(test_squirts_are_ignored_during_lockout);
    RUN_TEST(test_lockout_expires_after_cooldown);
    RUN_TEST(test_max_on_budget_resets_after_lockout);
    RUN_TEST(test_force_off_releases_immediately);
    RUN_TEST(test_force_off_does_not_wedge_the_relay);
    RUN_TEST(test_hold_survives_millis_rollover);

    return UNITY_END();
}
