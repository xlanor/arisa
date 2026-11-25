#include <arisa/provider/headless/memory_assets.hpp>
#include <arisa/provider/headless/recording_diagnostics.hpp>
#include <arisa/provider/sdl3/sdl3_audio.hpp>
#include <arisa/provider/sdl3/sdl3_browser.hpp>
#include <arisa/provider/sdl3/sdl3_clipboard.hpp>
#include <arisa/provider/sdl3/sdl3_context.hpp>
#include <arisa/provider/sdl3/sdl3_fonts.hpp>
#include <arisa/provider/sdl3/sdl3_haptics.hpp>
#include <arisa/provider/sdl3/sdl3_system_info.hpp>
#include <arisa/provider/standard/directory_assets.hpp>

#include "sdl3_test.hpp"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::provider::sdl3 {

class Sdl3ProvidersTest : public Sdl3Test {
 protected:
  static constexpr std::array face{std::byte{0x00}, std::byte{0x01},
                                   std::byte{0x00}, std::byte{0x00}};

  static auto path(std::string_view text) -> AssetPath {
    return *AssetPath::create(text);
  }
};

TEST_F(Sdl3ProvidersTest, ContextOwnsItsSubsystems) {
  {
    auto context = Sdl3Context::create(SDL_INIT_EVENTS);
    ASSERT_TRUE(context.has_value());
    EXPECT_EQ(context->subsystems(), SDL_INIT_EVENTS);
    EXPECT_NE(SDL_WasInit(SDL_INIT_EVENTS), 0U);
    const Sdl3Context moved{std::move(*context)};
    EXPECT_EQ(moved.subsystems(), SDL_INIT_EVENTS);
  }
  EXPECT_EQ(SDL_WasInit(0), 0U);
}

TEST_F(Sdl3ProvidersTest, ContextsNestAndOnlyTheLastQuits) {
  auto outer = Sdl3Context::create(SDL_INIT_EVENTS);
  ASSERT_TRUE(outer.has_value());
  {
    const auto inner = Sdl3Context::create(SDL_INIT_EVENTS);
    ASSERT_TRUE(inner.has_value());
  }
  EXPECT_NE(SDL_WasInit(SDL_INIT_EVENTS), 0U);
}

TEST_F(Sdl3ProvidersTest, ContextRejectsNoSubsystems) {
  const auto context = Sdl3Context::create(0);
  ASSERT_FALSE(context.has_value());
  EXPECT_EQ(context.error().code(), ProviderErrorCode::invalid_argument);
}

TEST_F(Sdl3ProvidersTest, BatteryStatesMapToBatteryResults) {
  const auto missing = Sdl3SystemInfo::interpret(SDL_POWERSTATE_NO_BATTERY, -1);
  ASSERT_FALSE(missing.has_value());
  EXPECT_EQ(missing.error().code(), ProviderErrorCode::not_found);
  const auto unknown = Sdl3SystemInfo::interpret(SDL_POWERSTATE_UNKNOWN, 40);
  ASSERT_FALSE(unknown.has_value());
  EXPECT_EQ(unknown.error().code(), ProviderErrorCode::not_found);
  const auto failed = Sdl3SystemInfo::interpret(SDL_POWERSTATE_ERROR, 40);
  ASSERT_FALSE(failed.has_value());
  EXPECT_EQ(failed.error().code(), ProviderErrorCode::platform_failure);
  const auto unmeasured =
      Sdl3SystemInfo::interpret(SDL_POWERSTATE_ON_BATTERY, -1);
  ASSERT_FALSE(unmeasured.has_value());
  EXPECT_EQ(unmeasured.error().code(), ProviderErrorCode::not_found);
  EXPECT_EQ(
      Sdl3SystemInfo::interpret(SDL_POWERSTATE_ON_BATTERY, 55),
      (BatteryState{.percent = BatteryPercent::of<55>(), .charging = false}));
  EXPECT_EQ(
      Sdl3SystemInfo::interpret(SDL_POWERSTATE_CHARGING, 140),
      (BatteryState{.percent = BatteryPercent::of<100>(), .charging = true}));
  EXPECT_EQ(
      Sdl3SystemInfo::interpret(SDL_POWERSTATE_CHARGED, 100),
      (BatteryState{.percent = BatteryPercent::of<100>(), .charging = true}));
}

TEST_F(Sdl3ProvidersTest, SystemInfoHasNoNetwork) {
  const auto info = Sdl3SystemInfo::create();
  ASSERT_TRUE(info.has_value());
  const auto network = info->network();
  ASSERT_FALSE(network.has_value());
  EXPECT_EQ(network.error().code(), ProviderErrorCode::unsupported_capability);
}

TEST_F(Sdl3ProvidersTest, DimmingInhibitFollowsTheVideoDriver) {
  auto info = Sdl3SystemInfo::create();
  ASSERT_TRUE(info.has_value());
  if (std::string_view{SDL_GetCurrentVideoDriver()} == "offscreen") {
    const auto inhibit = info->inhibit_dimming();
    ASSERT_FALSE(inhibit.has_value());
    EXPECT_EQ(inhibit.error().code(),
              ProviderErrorCode::unsupported_capability);
    return;
  }
  {
    const auto outer = info->inhibit_dimming();
    ASSERT_TRUE(outer.has_value());
    {
      const auto inner = info->inhibit_dimming();
      ASSERT_TRUE(inner.has_value());
      EXPECT_FALSE(SDL_ScreenSaverEnabled());
    }
    EXPECT_FALSE(SDL_ScreenSaverEnabled());
  }
  EXPECT_TRUE(SDL_ScreenSaverEnabled());
}

TEST_F(Sdl3ProvidersTest, HapticsSucceedWithoutAGamepad) {
  auto haptics = Sdl3Haptics::create();
  ASSERT_TRUE(haptics.has_value());
  for (const SensoryFeedback& feedback : SensoryFeedback::catalogue()) {
    EXPECT_TRUE(haptics->play(feedback, HapticStrength{}).has_value());
  }
}

TEST_F(Sdl3ProvidersTest, EverySensoryFeedbackRumbles) {
  for (const SensoryFeedback& feedback : SensoryFeedback::catalogue()) {
    const Sdl3Haptics::Rumble rumble =
        Sdl3Haptics::rumble(feedback, HapticStrength{});
    EXPECT_GT(rumble.duration.count(), 0);
    EXPECT_TRUE(rumble.low > 0 || rumble.high > 0);
  }
  EXPECT_GT(
      Sdl3Haptics::rumble(SensoryFeedback::error, HapticStrength{}).low,
      Sdl3Haptics::rumble(SensoryFeedback::selection, HapticStrength{}).low);
}

TEST_F(Sdl3ProvidersTest, HapticStrengthScalesTheRumbleUpToFull) {
  const Sdl3Haptics::Rumble normal =
      Sdl3Haptics::rumble(SensoryFeedback::selection, HapticStrength{});
  const Sdl3Haptics::Rumble doubled = Sdl3Haptics::rumble(
      SensoryFeedback::selection, HapticStrength::from_percent(200));
  EXPECT_NEAR(doubled.high, normal.high * 2, 2);
  EXPECT_EQ(doubled.duration, normal.duration);
  const Sdl3Haptics::Rumble muted = Sdl3Haptics::rumble(
      SensoryFeedback::selection, HapticStrength::from_percent(0));
  EXPECT_EQ(muted.low, 0);
  EXPECT_EQ(muted.high, 0);
}

TEST_F(Sdl3ProvidersTest, RawRumbleMapsClampedAmplitudesToFullScale) {
  const Sdl3Haptics::Rumble full = Sdl3Haptics::raw(
      RumbleValue{RumbleFrequency{160.0F}, RumbleFrequency{320.0F},
                  RumbleAmplitude{2.0F}, RumbleAmplitude{0.5F}});
  EXPECT_EQ(full.low, 65535);
  EXPECT_NEAR(full.high, 32767, 1);
  EXPECT_EQ(full.duration, Sdl3Haptics::raw_hold);
  const Sdl3Haptics::Rumble silent = Sdl3Haptics::raw(RumbleValue::silent());
  EXPECT_EQ(silent.low, 0);
  EXPECT_EQ(silent.high, 0);
  auto haptics = Sdl3Haptics::create();
  ASSERT_TRUE(haptics.has_value());
  EXPECT_TRUE(haptics
                  ->rumble(RumblePlayer::primary(), RumbleFrequency{160.0F},
                           RumbleFrequency{320.0F}, RumbleAmplitude{0.2F},
                           RumbleAmplitude{0.2F})
                  .has_value());
}

TEST_F(Sdl3ProvidersTest, AudioPlaysLoadedAndMissingSoundsAlike) {
  const standard::DirectoryAssets assets{ARISA_TEST_FIXTURES};
  headless::RecordingDiagnostics diagnostics;
  auto audio = Sdl3Audio::create(assets, diagnostics);
  ASSERT_TRUE(audio.has_value());
  EXPECT_EQ(diagnostics.snapshot().size(), all_sound_roles.size() - 1);
  for (int round = 0; round < 3; ++round) {
    for (const SoundRole role : all_sound_roles) {
      EXPECT_TRUE(audio->play(role).has_value());
    }
  }
  Sdl3Audio moved{std::move(*audio)};
  EXPECT_TRUE(moved.play(SoundRole::click).has_value());
}

TEST_F(Sdl3ProvidersTest, ClipboardRoundTripsOrReportsHonestly) {
  auto clipboard = Sdl3Clipboard::create();
  ASSERT_TRUE(clipboard.has_value());
  const auto written = clipboard->write_text("arisa-sdl3");
  if (!written.has_value()) {
    EXPECT_TRUE(written.error().code() ==
                    ProviderErrorCode::unsupported_capability ||
                written.error().code() == ProviderErrorCode::platform_failure);
    return;
  }
  const auto read = clipboard->read_text();
  ASSERT_TRUE(read.has_value());
  EXPECT_EQ(*read, "arisa-sdl3");
}

TEST_F(Sdl3ProvidersTest, ClipboardRejectsEmbeddedNul) {
  auto clipboard = Sdl3Clipboard::create();
  ASSERT_TRUE(clipboard.has_value());
  const auto written = clipboard->write_text(std::string_view{"a\0b", 3});
  ASSERT_FALSE(written.has_value());
  EXPECT_EQ(written.error().code(), ProviderErrorCode::invalid_argument);
}

TEST_F(Sdl3ProvidersTest, BrowserIsAlwaysAvailable) {
  EXPECT_TRUE(Sdl3Browser::create().has_value());
}

TEST_F(Sdl3ProvidersTest, FontsNeedTheStandardFace) {
  headless::MemoryAssets assets;
  assets.add(path("arisa/fonts/korean.ttf"), face);
  const auto fonts = Sdl3Fonts::create(assets);
  ASSERT_FALSE(fonts.has_value());
  EXPECT_EQ(fonts.error().code(), ProviderErrorCode::not_found);
}

TEST_F(Sdl3ProvidersTest, FontsServeLoadedFacesOnly) {
  headless::MemoryAssets assets;
  assets.add(path("arisa/fonts/standard.ttf"), face);
  assets.add(path("arisa/fonts/chinese_traditional.ttf"),
             std::span<const std::byte>{});
  auto fonts = Sdl3Fonts::create(assets);
  ASSERT_TRUE(fonts.has_value());
  const Sdl3Fonts moved{std::move(*fonts)};
  const auto standard = moved.system_font(SystemFont::standard);
  ASSERT_TRUE(standard.has_value());
  EXPECT_TRUE(std::ranges::equal(standard->bytes(), face));
  EXPECT_TRUE(moved.available(SystemFont::standard));
  EXPECT_FALSE(moved.available(SystemFont::chinese_traditional));
  const auto korean = moved.system_font(SystemFont::korean);
  ASSERT_FALSE(korean.has_value());
  EXPECT_EQ(korean.error().code(), ProviderErrorCode::not_found);
}

TEST_F(Sdl3ProvidersTest, FontsFollowTheBank) {
  headless::MemoryAssets assets;
  assets.add(path("ui/inter.otf"), face);
  const FontBank bank =
      FontBank{}.with(SystemFont::standard, path("ui/inter.otf"));
  const auto fonts = Sdl3Fonts::create(assets, bank);
  ASSERT_TRUE(fonts.has_value());
  EXPECT_TRUE(fonts->available(SystemFont::standard));
}

}
