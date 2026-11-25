#include <arisa/provider/gpu/frame_slots.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <optional>

namespace arisa::provider {

TEST(FrameSlotsTest, AwaitReturnsTheNextSlotWhileFramesAreAllowed) {
  FrameSlots slots{2};
  EXPECT_EQ(slots.await(), std::optional<std::uint32_t>{0});
}

TEST(FrameSlotsTest, InterruptWakesAnAwaitThatNoReleaseWillEnd) {
  FrameSlots slots{2};
  ASSERT_TRUE(slots.claim().has_value());
  slots.seal();
  std::future<std::optional<std::uint32_t>> waiting =
      std::async(std::launch::async, [&slots] { return slots.await(); });
  EXPECT_EQ(waiting.wait_for(std::chrono::milliseconds{50}),
            std::future_status::timeout);
  slots.interrupt();
  ASSERT_EQ(waiting.wait_for(std::chrono::seconds{5}),
            std::future_status::ready);
  EXPECT_EQ(waiting.get(), std::nullopt);
}

TEST(FrameSlotsTest, ReleaseStillEndsAnAwaitWithTheFreedSlot) {
  FrameSlots slots{2};
  ASSERT_TRUE(slots.claim().has_value());
  slots.seal();
  std::future<std::optional<std::uint32_t>> waiting =
      std::async(std::launch::async, [&slots] { return slots.await(); });
  slots.release(0);
  ASSERT_EQ(waiting.wait_for(std::chrono::seconds{5}),
            std::future_status::ready);
  EXPECT_EQ(waiting.get(), std::optional<std::uint32_t>{1});
}

}
