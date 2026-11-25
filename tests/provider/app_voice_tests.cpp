#include <arisa/provider/audio/app_voice.hpp>
#include <arisa/provider/audio/voice_slots.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace arisa::provider {

using namespace std::chrono_literals;

class VoiceSlotsTest : public ::testing::Test {
 protected:
  static constexpr auto accept = [] { return true; };
};

TEST_F(VoiceSlotsTest, BuffersCycleFromWritingThroughQueuedBackToFree) {
  VoiceSlots slots{2};
  const auto first = slots.take(std::nullopt);
  const auto second = slots.take(std::nullopt);
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_NE(*first, *second);
  EXPECT_FALSE(slots.take(std::nullopt).has_value());
  EXPECT_TRUE(slots.queue(*first, 480, accept).has_value());
  EXPECT_FALSE(slots.queue(*first, 480, accept).has_value());
  EXPECT_EQ(slots.state(*first), VoiceSlots::State::queued);
  EXPECT_EQ(slots.statistics().latency, 10ms);
  slots.played(*first);
  EXPECT_EQ(slots.state(*first), VoiceSlots::State::free);
  const AppVoiceStatistics counted = slots.statistics();
  EXPECT_EQ(counted.submitted, 1U);
  EXPECT_EQ(counted.played, 1U);
  EXPECT_EQ(counted.underruns, 1U);
  EXPECT_EQ(counted.queued, 0U);
}

TEST_F(VoiceSlotsTest, AStarvedQueueCountsOneUnderrunPerEmptying) {
  VoiceSlots slots{3};
  std::vector<std::uint32_t> taken;
  for (int index = 0; index < 3; ++index) {
    taken.push_back(*slots.take(std::nullopt));
    ASSERT_TRUE(slots.queue(taken.back(), 960, accept).has_value());
  }
  EXPECT_EQ(slots.statistics().queued, 3U);
  EXPECT_EQ(slots.statistics().latency, 60ms);
  slots.played(taken[0]);
  slots.played(taken[1]);
  EXPECT_EQ(slots.statistics().underruns, 0U);
  slots.played(taken[2]);
  EXPECT_EQ(slots.statistics().underruns, 1U);
}

TEST_F(VoiceSlotsTest, GivingBackAndAFailedEnqueueFreeTheSlot) {
  VoiceSlots slots{1};
  const auto slot = slots.take(std::nullopt);
  ASSERT_TRUE(slot.has_value());
  slots.give_back(*slot);
  EXPECT_EQ(slots.state(*slot), VoiceSlots::State::free);
  const auto again = slots.take(std::nullopt);
  ASSERT_TRUE(again.has_value());
  const auto refused = slots.queue(*again, 64, [] { return false; });
  ASSERT_FALSE(refused.has_value());
  EXPECT_EQ(refused.error().code(), ProviderErrorCode::platform_failure);
  EXPECT_EQ(slots.state(*again), VoiceSlots::State::free);
  EXPECT_EQ(slots.statistics().submitted, 0U);
}

TEST_F(VoiceSlotsTest, AReusedSlotIsNotReapedOnItsPreviousBuffersDoneState) {
  enum class Wave : std::uint8_t { free, queued, done };
  VoiceSlots slots{1};
  std::vector<Wave> waves(1, Wave::free);
  const auto finished = [&waves](std::uint32_t slot) {
    return waves[slot] == Wave::done;
  };
  const auto first = slots.take(std::nullopt);
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(slots
                  .queue(*first, 64,
                         [&] {
                           waves[*first] = Wave::queued;
                           return true;
                         })
                  .has_value());
  slots.reap(finished);
  EXPECT_EQ(slots.state(*first), VoiceSlots::State::queued);
  waves[*first] = Wave::done;
  slots.reap(finished);
  EXPECT_EQ(slots.state(*first), VoiceSlots::State::free);
  const auto reused = slots.take(std::nullopt);
  ASSERT_TRUE(reused.has_value());
  EXPECT_EQ(*reused, *first);
  slots.reap(finished);
  EXPECT_EQ(slots.state(*reused), VoiceSlots::State::writing);
  ASSERT_TRUE(slots
                  .queue(*reused, 64,
                         [&] {
                           waves[*reused] = Wave::queued;
                           return true;
                         })
                  .has_value());
  slots.reap(finished);
  EXPECT_EQ(slots.state(*reused), VoiceSlots::State::queued);
  EXPECT_EQ(slots.statistics().played, 1U);
}

TEST_F(VoiceSlotsTest, WaitingTimesOutAndClosingWakesWaiters) {
  VoiceSlots slots{1};
  ASSERT_TRUE(slots.take(std::nullopt).has_value());
  EXPECT_FALSE(slots.take(1ms).has_value());
  slots.close();
  EXPECT_FALSE(slots.take(1s).has_value());
}

class FakeVoice final : public AppVoice::Implementation {
 public:
  explicit FakeVoice(std::uint32_t count, std::uint32_t frames)
      : slots_(count),
        frames_(frames),
        storage_(static_cast<std::size_t>(count) * frames *
                 AppVoice::channels) {}

  auto take(std::optional<std::chrono::nanoseconds> timeout)
      -> std::optional<std::uint32_t> override {
    return slots_.take(timeout);
  }

  auto samples(std::uint32_t slot) noexcept
      -> std::span<std::int16_t> override {
    const std::size_t width =
        static_cast<std::size_t>(frames_) * AppVoice::channels;
    return std::span<std::int16_t>{storage_}.subspan(slot * width, width);
  }

  auto submit(std::uint32_t slot, std::uint32_t frames)
      -> std::expected<void, ProviderError> override {
    return slots_.queue(slot, frames, [&] {
      submitted.push_back(samples(slot)[0]);
      return true;
    });
  }

  void give_back(std::uint32_t slot) noexcept override {
    slots_.give_back(slot);
  }

  auto statistics() const -> AppVoiceStatistics override {
    return slots_.statistics();
  }

  void set_ui_sounds(UiSounds sounds) override { ui = sounds; }

  VoiceSlots slots_;
  std::uint32_t frames_;
  std::vector<std::int16_t> storage_;
  std::vector<std::int16_t> submitted;
  UiSounds ui{UiSounds::play};
};

TEST_F(VoiceSlotsTest, AppVoiceHandsOutWritableSlicesInPlace) {
  auto fake = std::make_shared<FakeVoice>(2, 480);
  AppVoice voice{fake, AppVoiceOptions{}};
  std::optional<AppVoice::Buffer> buffer = voice.acquire();
  ASSERT_TRUE(buffer.has_value());
  EXPECT_EQ(buffer->frames(), 480U);
  EXPECT_EQ(buffer->samples().size(), 960U);
  buffer->samples()[0] = 1234;
  EXPECT_EQ(fake->storage_[buffer->samples().data() - fake->storage_.data()],
            1234);
  ASSERT_TRUE(buffer->submit(240).has_value());
  EXPECT_FALSE(buffer->submit().has_value());
  EXPECT_EQ(fake->submitted, (std::vector<std::int16_t>{1234}));
  EXPECT_EQ(voice.statistics().latency, 5ms);
  voice.set_ui_sounds(UiSounds::mute);
  EXPECT_EQ(fake->ui, UiSounds::mute);
}

TEST_F(VoiceSlotsTest, DroppingAnUnsubmittedBufferReturnsIt) {
  auto fake = std::make_shared<FakeVoice>(1, 64);
  AppVoice voice{fake, AppVoiceOptions{}};
  {
    std::optional<AppVoice::Buffer> buffer = voice.acquire();
    ASSERT_TRUE(buffer.has_value());
    EXPECT_FALSE(voice.acquire().has_value());
  }
  EXPECT_TRUE(voice.acquire().has_value());
}

TEST_F(VoiceSlotsTest, SubmittingMoreFramesThanTheSliceIsRefused) {
  auto fake = std::make_shared<FakeVoice>(1, 64);
  AppVoice voice{fake, AppVoiceOptions{}};
  std::optional<AppVoice::Buffer> buffer = voice.acquire();
  ASSERT_TRUE(buffer.has_value());
  EXPECT_FALSE(buffer->submit(65).has_value());
  EXPECT_FALSE(buffer->submit(0).has_value());
  EXPECT_TRUE(buffer->submit(64).has_value());
}

}
