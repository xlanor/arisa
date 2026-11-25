#include <arisa/async/frame_timeline.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <stop_token>
#include <vector>

namespace arisa::async {

using namespace std::chrono_literals;

class FrameTimelineTest : public ::testing::Test {
 protected:
  std::shared_ptr<std::chrono::nanoseconds> now_{
      std::make_shared<std::chrono::nanoseconds>(0)};
  FrameTimeline timeline_{[now = now_] { return *now; }};
};

TEST_F(FrameTimelineTest, FiresTimersOnceTheirDeadlinePasses) {
  std::vector<int> fired;
  Job late = timeline_.after(
      20ms, [&fired](const std::stop_token&) { fired.push_back(20); });
  Job early = timeline_.after(
      10ms, [&fired](const std::stop_token&) { fired.push_back(10); });
  EXPECT_EQ(timeline_.fire(), 0U);
  *now_ = 25ms;
  EXPECT_EQ(timeline_.fire(), 2U);
  EXPECT_EQ(fired, (std::vector<int>{10, 20}));
}

TEST_F(FrameTimelineTest, NextFrameWaitersRunOnTheFollowingFire) {
  int runs = 0;
  Job outer = timeline_.next_frame([&](const std::stop_token&) { ++runs; });
  EXPECT_EQ(timeline_.pending(), 1U);
  EXPECT_EQ(timeline_.fire(), 1U);
  EXPECT_EQ(runs, 1);
  EXPECT_EQ(timeline_.fire(), 0U);
}

TEST_F(FrameTimelineTest, DroppedJobsNeverFireAndAreForgotten) {
  int runs = 0;
  {
    Job timer =
        timeline_.after(1ms, [&runs](const std::stop_token&) { ++runs; });
    Job waiter =
        timeline_.next_frame([&runs](const std::stop_token&) { ++runs; });
  }
  EXPECT_EQ(timeline_.pending(), 0U);
  *now_ = 5ms;
  EXPECT_EQ(timeline_.fire(), 0U);
  EXPECT_EQ(runs, 0);
}

TEST_F(FrameTimelineTest, ClosingDropsEverythingAndRefusesMore) {
  int runs = 0;
  Job timer = timeline_.after(1ms, [&runs](const std::stop_token&) { ++runs; });
  timeline_.close();
  Job late = timeline_.next_frame([&runs](const std::stop_token&) { ++runs; });
  *now_ = 5ms;
  EXPECT_EQ(timeline_.fire(), 0U);
  EXPECT_EQ(timeline_.pending(), 0U);
  EXPECT_EQ(runs, 0);
  EXPECT_FALSE(timer.started());
}

}
