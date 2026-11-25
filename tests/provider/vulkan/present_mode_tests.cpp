#include <arisa/provider/vulkan/present_mode.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

namespace arisa::provider::vulkan {

class PresentModeTest : public ::testing::Test {
 protected:
  static constexpr std::array<VkPresentModeKHR, 3> every{
      VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR,
      VK_PRESENT_MODE_MAILBOX_KHR};
  static constexpr std::array<VkPresentModeKHR, 2> immediate{
      VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR};
  static constexpr std::array<VkPresentModeKHR, 1> fifo{
      VK_PRESENT_MODE_FIFO_KHR};
};

TEST_F(PresentModeTest, AnyThrottledIntervalPresentsInFifo) {
  for (const std::uint32_t interval : {1U, 2U, 4U}) {
    EXPECT_EQ(PresentMode::choose(interval, every), VK_PRESENT_MODE_FIFO_KHR);
  }
}

TEST_F(PresentModeTest, IntervalZeroPrefersMailboxThenImmediate) {
  EXPECT_EQ(PresentMode::choose(0, every), VK_PRESENT_MODE_MAILBOX_KHR);
  EXPECT_EQ(PresentMode::choose(0, immediate), VK_PRESENT_MODE_IMMEDIATE_KHR);
  EXPECT_EQ(PresentMode::choose(0, fifo), VK_PRESENT_MODE_FIFO_KHR);
}

TEST_F(PresentModeTest, OnlyCrossingZeroRebuildsTheSwapchain) {
  EXPECT_TRUE(PresentMode::rebuilds(1, 0));
  EXPECT_TRUE(PresentMode::rebuilds(0, 2));
  EXPECT_FALSE(PresentMode::rebuilds(1, 2));
  EXPECT_FALSE(PresentMode::rebuilds(0, 0));
  EXPECT_FALSE(PresentMode::rebuilds(3, 3));
}

}
