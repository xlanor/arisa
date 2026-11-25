#include <arisa/provider/gpu/frame_schedule.hpp>
#include <arisa/provider/gpu/gpu_takeover.hpp>
#include <arisa/provider/gpu/render_queue.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <optional>

#include <memory>
#include <utility>
#include <vector>

namespace arisa::provider {

class GpuTakeoverTest : public ::testing::Test {
 protected:
  auto claim(std::vector<int>& order)
      -> std::expected<GpuTakeover, ProviderError> {
    return seat_.claim(queue_, schedule_, [this, &order]() {
      order.push_back(queue_.parked() ? 1 : 0);
      return support::ScopedCallback{[this, &order]() noexcept {
        order.push_back(queue_.parked() ? 3 : 2);
      }};
    });
  }

  RenderQueue queue_;
  std::shared_ptr<FrameSchedule> schedule_ = std::make_shared<FrameSchedule>();
  GpuTakeover::Seat seat_;
};

TEST_F(GpuTakeoverTest, ClaimParksTheQueueAndReleaseHandsItBack) {
  std::vector<int> order;
  auto taken = claim(order);
  ASSERT_TRUE(taken.has_value());
  EXPECT_TRUE(taken->held());
  EXPECT_TRUE(seat_.held());
  EXPECT_TRUE(queue_.parked());
  EXPECT_FALSE(schedule_->take_request());
  taken->release();
  EXPECT_FALSE(taken->held());
  EXPECT_FALSE(seat_.held());
  EXPECT_FALSE(queue_.parked());
  EXPECT_TRUE(schedule_->take_request());
  EXPECT_EQ(order, (std::vector<int>{1, 3}));
  EXPECT_EQ(seat_.counters(),
            (GpuTakeover::Counters{.granted = 1, .returned = 1, .refused = 0}));
}

TEST_F(GpuTakeoverTest, OnlyOneTakeoverAtATime) {
  std::vector<int> order;
  auto first = claim(order);
  ASSERT_TRUE(first.has_value());
  auto second = claim(order);
  ASSERT_FALSE(second.has_value());
  EXPECT_EQ(second.error().detail(), ProviderErrorDetail::taken_over);
  first->release();
  auto third = claim(order);
  EXPECT_TRUE(third.has_value());
  EXPECT_EQ(seat_.counters().refused, 1U);
  EXPECT_EQ(seat_.counters().granted, 2U);
}

TEST_F(GpuTakeoverTest, AppLeasesStillWorkWhileArisaIsParked) {
  std::vector<int> order;
  auto taken = claim(order);
  ASSERT_TRUE(taken.has_value());
  std::optional<RenderQueue::Lease> lease = queue_.try_lease();
  ASSERT_TRUE(lease.has_value());
  EXPECT_TRUE(lease->held());
}

TEST_F(GpuTakeoverTest, MovingKeepsOneHolder) {
  std::vector<int> order;
  auto taken = claim(order);
  ASSERT_TRUE(taken.has_value());
  GpuTakeover moved{std::move(*taken)};
  EXPECT_FALSE(taken->held());
  EXPECT_TRUE(moved.held());
  EXPECT_TRUE(queue_.parked());
  GpuTakeover assigned;
  assigned = std::move(moved);
  EXPECT_TRUE(queue_.parked());
  EXPECT_EQ(order.size(), 1U);
  assigned.release();
  EXPECT_FALSE(queue_.parked());
  EXPECT_EQ(order.size(), 2U);
}

TEST_F(GpuTakeoverTest, DestroyingTheTakeoverHandsBack) {
  std::vector<int> order;
  {
    auto taken = claim(order);
    ASSERT_TRUE(taken.has_value());
  }
  EXPECT_FALSE(seat_.held());
  EXPECT_FALSE(queue_.parked());
  EXPECT_EQ(seat_.counters().returned, 1U);
}

TEST_F(GpuTakeoverTest, AGoneScheduleIsHarmless) {
  std::vector<int> order;
  auto taken = claim(order);
  ASSERT_TRUE(taken.has_value());
  schedule_.reset();
  taken->release();
  EXPECT_FALSE(queue_.parked());
}

TEST_F(GpuTakeoverTest, SubmissionsAreRefusedInsteadOfWaitingWhileTakenOver) {
  std::vector<int> order;
  auto taken = claim(order);
  ASSERT_TRUE(taken.has_value());
  std::future<bool> refused = std::async(std::launch::async, [this] {
    return !queue_.submit().has_value();
  });
  ASSERT_EQ(refused.wait_for(std::chrono::seconds{5}),
            std::future_status::ready);
  EXPECT_TRUE(refused.get());
  taken->release();
  EXPECT_TRUE(queue_.submit().has_value());
}

TEST_F(GpuTakeoverTest, ASubmissionAlreadyWaitingIsReleasedByTheTakeover) {
  std::optional<RenderQueue::Lease> app{queue_.lease()};
  std::future<bool> refused = std::async(std::launch::async, [this] {
    return !queue_.submit().has_value();
  });
  EXPECT_EQ(refused.wait_for(std::chrono::milliseconds{50}),
            std::future_status::timeout);
  std::vector<int> order;
  auto taken = claim(order);
  ASSERT_TRUE(taken.has_value());
  app.reset();
  ASSERT_EQ(refused.wait_for(std::chrono::seconds{5}),
            std::future_status::ready);
  EXPECT_TRUE(refused.get());
}

}
