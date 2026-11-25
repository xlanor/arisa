#include <arisa/provider/gpu/lent_swapchain.hpp>
#include <arisa/provider/gpu/swap_interval.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

namespace arisa::provider {

class LentSwapchainTest : public ::testing::Test {};

TEST_F(LentSwapchainTest, NothingIsAcquirableUntilTheSwapchainIsLent) {
  LentSwapchain<> lent;
  EXPECT_FALSE(lent.lent());
  EXPECT_FALSE(lent.acquire(0));
  lent.lend(2);
  EXPECT_TRUE(lent.lent());
  EXPECT_TRUE(lent.acquire(0));
  EXPECT_FALSE(lent.acquire(2));
}

TEST_F(LentSwapchainTest, AnImageIsAcquiredOnceAndPresentedOnce) {
  LentSwapchain<> lent;
  lent.lend(2);
  EXPECT_FALSE(lent.present(0));
  EXPECT_TRUE(lent.acquire(0));
  EXPECT_FALSE(lent.acquire(0));
  EXPECT_TRUE(lent.acquired(0));
  EXPECT_EQ(lent.outstanding(), 1U);
  EXPECT_FALSE(lent.present(1));
  EXPECT_TRUE(lent.present(0));
  EXPECT_FALSE(lent.present(0));
  EXPECT_EQ(lent.outstanding(), 0U);
}

TEST_F(LentSwapchainTest,
       SettlingHandsOverEveryOutstandingImageWithItsPayload) {
  LentSwapchain<std::uint64_t> lent;
  lent.lend(3);
  ASSERT_TRUE(lent.acquire(2, 22));
  ASSERT_TRUE(lent.acquire(0, 10));
  ASSERT_TRUE(lent.acquire(1, 11));
  ASSERT_TRUE(lent.present(1));
  const LentSwapchain<std::uint64_t>::Settlement settled = lent.settle();
  ASSERT_EQ(settled.outstanding.size(), 2U);
  EXPECT_EQ(settled.outstanding[0].index, 0U);
  EXPECT_EQ(settled.outstanding[0].payload, 10U);
  EXPECT_EQ(settled.outstanding[1].index, 2U);
  EXPECT_EQ(settled.outstanding[1].payload, 22U);
  EXPECT_FALSE(lent.lent());
  EXPECT_FALSE(lent.present(0));
  EXPECT_FALSE(lent.acquire(0, 0));
}

TEST_F(LentSwapchainTest,
       AnIntervalAskedForDuringTheLoanWaitsForTheSettlement) {
  LentSwapchain<> lent;
  EXPECT_EQ(lent.request(SwapInterval::of<2>()), SwapInterval::of<2>());
  lent.lend(2);
  EXPECT_FALSE(lent.request(SwapInterval::of<3>()).has_value());
  EXPECT_FALSE(lent.request(SwapInterval::of<4>()).has_value());
  const LentSwapchain<>::Settlement settled = lent.settle();
  EXPECT_EQ(settled.interval, SwapInterval::of<4>());
  EXPECT_EQ(lent.request(SwapInterval::of<1>()), SwapInterval::of<1>());
  lent.lend(2);
  EXPECT_FALSE(lent.settle().interval.has_value());
}

TEST_F(LentSwapchainTest, ResizingWaitsUntilNothingIsOutstanding) {
  LentSwapchain<> lent;
  lent.lend(2);
  ASSERT_TRUE(lent.acquire(1));
  EXPECT_FALSE(lent.resize(4));
  EXPECT_FALSE(lent.acquire(3));
  ASSERT_TRUE(lent.present(1));
  EXPECT_TRUE(lent.resize(4));
  EXPECT_TRUE(lent.acquire(3));
}

}
