#include <arisa/provider/input/navigation_tuning.hpp>

#include <interaction/press_tracker.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>

namespace arisa::interaction {

using std::chrono::milliseconds;

class PressTrackerTest : public ::testing::Test {
 protected:
  static auto repeats_until(PressTracker& tracker, milliseconds end,
                            const provider::NavigationTuning& tuning)
      -> std::uint32_t {
    std::uint32_t total = 0;
    for (milliseconds now{1}; now <= end; now += milliseconds{1}) {
      total += tracker.update(true, now, tuning).repeats;
    }
    return total;
  }
};

TEST_F(PressTrackerTest, IdleIgnoresReleaseAndTime) {
  PressTracker tracker;
  const provider::NavigationTuning tuning;
  const PressTracker::Events events =
      tracker.update(false, milliseconds{100}, tuning);
  EXPECT_FALSE(events.pressed);
  EXPECT_FALSE(events.released);
  EXPECT_EQ(events.repeats, 0U);
  EXPECT_FALSE(tracker.held());
}

TEST_F(PressTrackerTest, PressFiresOnceAndRepeatsAfterTheDelay) {
  PressTracker tracker;
  const provider::NavigationTuning tuning;
  const PressTracker::Events pressed =
      tracker.update(true, milliseconds{0}, tuning);
  EXPECT_TRUE(pressed.pressed);
  EXPECT_EQ(pressed.repeats, 0U);
  EXPECT_TRUE(tracker.held());
  EXPECT_FALSE(tracker.repeating());
  EXPECT_EQ(repeats_until(tracker, milliseconds{399}, tuning), 0U);
  EXPECT_EQ(tracker.update(true, milliseconds{400}, tuning).repeats, 1U);
  EXPECT_TRUE(tracker.repeating());
}

TEST_F(PressTrackerTest, RepeatsAccelerateToTheMinimumInterval) {
  PressTracker tracker;
  const provider::NavigationTuning tuning;
  static_cast<void>(tracker.update(true, milliseconds{0}, tuning));
  EXPECT_EQ(repeats_until(tracker, milliseconds{1000}, tuning), 10U);
  EXPECT_EQ(repeats_until(tracker, milliseconds{1000}, tuning), 0U);
  PressTracker steady;
  static_cast<void>(steady.update(true, milliseconds{0}, tuning));
  static_cast<void>(steady.update(true, milliseconds{1000}, tuning));
  EXPECT_EQ(steady.update(true, milliseconds{1400}, tuning).repeats, 10U);
}

TEST_F(PressTrackerTest, ReleaseReturnsToIdleAndRestartsTiming) {
  PressTracker tracker;
  const provider::NavigationTuning tuning;
  static_cast<void>(tracker.update(true, milliseconds{0}, tuning));
  static_cast<void>(tracker.update(true, milliseconds{500}, tuning));
  const PressTracker::Events released =
      tracker.update(false, milliseconds{510}, tuning);
  EXPECT_TRUE(released.released);
  EXPECT_FALSE(tracker.held());
  EXPECT_TRUE(tracker.update(true, milliseconds{600}, tuning).pressed);
  EXPECT_EQ(tracker.update(true, milliseconds{900}, tuning).repeats, 0U);
  EXPECT_EQ(tracker.update(true, milliseconds{1000}, tuning).repeats, 1U);
}

TEST_F(PressTrackerTest, LongPressFiresOncePerHold) {
  PressTracker tracker;
  const provider::NavigationTuning tuning;
  static_cast<void>(tracker.update(true, milliseconds{0}, tuning));
  EXPECT_FALSE(tracker.update(true, milliseconds{499}, tuning).long_press);
  EXPECT_TRUE(tracker.update(true, milliseconds{500}, tuning).long_press);
  EXPECT_FALSE(tracker.update(true, milliseconds{900}, tuning).long_press);
  static_cast<void>(tracker.update(false, milliseconds{950}, tuning));
  static_cast<void>(tracker.update(true, milliseconds{1000}, tuning));
  EXPECT_TRUE(tracker.update(true, milliseconds{1500}, tuning).long_press);
}

}
