#include <arisa/provider/audio/app_voice.hpp>
#include <arisa/provider/headless/recording_diagnostics.hpp>
#include <arisa/provider/sdl3/sdl3_audio.hpp>
#include <arisa/provider/standard/directory_assets.hpp>
#include <arisa/provider/standard/std_threading.hpp>

#include "sdl3_test.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <thread>
#include <utility>

namespace arisa::provider::sdl3 {

using namespace std::chrono_literals;

class Sdl3AppVoiceTest : public Sdl3Test {
 protected:
  auto audio() -> std::optional<Sdl3Audio> {
    auto created =
        Sdl3Audio::create(assets_, diagnostics_, {}, std::ref(threading_));
    if (!created.has_value()) {
      return std::nullopt;
    }
    return std::move(*created);
  }

  static void fill(AppVoice::Buffer& buffer, std::int16_t level) {
    for (std::int16_t& sample : buffer.samples()) {
      sample = level;
    }
  }

  standard::DirectoryAssets assets_{ARISA_TEST_FIXTURES};
  headless::RecordingDiagnostics diagnostics_;
  standard::StdThreadingProvider threading_;
};

TEST_F(Sdl3AppVoiceTest, OpensOneVoiceAndDucksUiSounds) {
  auto made = audio();
  ASSERT_TRUE(made.has_value());
  EXPECT_FLOAT_EQ(made->ui_gain(), 1.0F);
  {
    auto voice = made->open_voice(AppVoiceOptions{});
    ASSERT_TRUE(voice.has_value());
    EXPECT_FLOAT_EQ(made->ui_gain(), AppVoice::ducked_gain);
    const auto second = made->open_voice(AppVoiceOptions{});
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error().detail(), ProviderErrorDetail::voice_open);
    voice->set_ui_sounds(UiSounds::mute);
    EXPECT_FLOAT_EQ(made->ui_gain(), 0.0F);
    EXPECT_TRUE(made->play(SoundRole::click).has_value());
  }
  EXPECT_FLOAT_EQ(made->ui_gain(), 1.0F);
  EXPECT_TRUE(made->open_voice(AppVoiceOptions{}).has_value());
}

TEST_F(Sdl3AppVoiceTest, SubmittedBuffersPlayOutAndComeBack) {
  auto made = audio();
  ASSERT_TRUE(made.has_value());
  auto voice = made->open_voice(
      AppVoiceOptions{.buffer_frames = AudioFrames::of<480>(),
                      .buffer_count = AudioBufferCount::of<3>(),
                      .ui_sounds = UiSounds::play});
  ASSERT_TRUE(voice.has_value());
  EXPECT_FLOAT_EQ(made->ui_gain(), 1.0F);
  constexpr int rounds = 12;
  for (int round = 0; round < rounds; ++round) {
    std::optional<AppVoice::Buffer> buffer = voice->acquire(2s);
    ASSERT_TRUE(buffer.has_value()) << round;
    EXPECT_EQ(buffer->frames(), 480U);
    fill(*buffer, static_cast<std::int16_t>(round * 100));
    ASSERT_TRUE(buffer->submit().has_value());
  }
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (voice->statistics().played < rounds &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(5ms);
  }
  const AppVoiceStatistics counted = voice->statistics();
  EXPECT_EQ(counted.submitted, static_cast<std::uint64_t>(rounds));
  EXPECT_EQ(counted.played, static_cast<std::uint64_t>(rounds));
  EXPECT_EQ(counted.queued, 0U);
  EXPECT_GE(counted.underruns, 1U);
  EXPECT_EQ(counted.latency, 0ns);
}

TEST_F(Sdl3AppVoiceTest, LatencyReportsTheQueuedAudio) {
  auto made = audio();
  ASSERT_TRUE(made.has_value());
  auto voice = made->open_voice(
      AppVoiceOptions{.buffer_frames = AudioFrames::of<4800>(),
                      .buffer_count = AudioBufferCount::of<2>(),
                      .ui_sounds = UiSounds::duck});
  ASSERT_TRUE(voice.has_value());
  for (int index = 0; index < 2; ++index) {
    std::optional<AppVoice::Buffer> buffer = voice->acquire();
    ASSERT_TRUE(buffer.has_value());
    fill(*buffer, 0);
    ASSERT_TRUE(buffer->submit().has_value());
  }
  const AppVoiceStatistics counted = voice->statistics();
  EXPECT_LE(counted.latency, 200ms);
  EXPECT_GT(counted.latency, 0ns);
}

TEST_F(Sdl3AppVoiceTest, WithoutThreadingTheVoiceIsUnsupported) {
  auto created = Sdl3Audio::create(assets_, diagnostics_);
  ASSERT_TRUE(created.has_value());
  const auto voice = created->open_voice(AppVoiceOptions{});
  ASSERT_FALSE(voice.has_value());
  EXPECT_EQ(voice.error().code(), ProviderErrorCode::unsupported_capability);
}

}
