#include <arisa/provider/headless/headless_factory.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::provider::headless {

using namespace std::chrono_literals;

TEST(ManualClockTest, AdvancesOnlyWhenTold) {
  ManualClock clock;
  const Instant start = clock.now();
  EXPECT_EQ(clock.now(), start);
  ASSERT_TRUE(clock.advance(16ms).has_value());
  EXPECT_EQ(clock.now() - start, 16ms);
  EXPECT_EQ(
      clock.wall_now(),
      std::chrono::system_clock::time_point{ManualClock::default_wall} + 16ms);
}

TEST(ManualClockTest, RejectsNegativeSteps) {
  ManualClock clock;
  const auto result = clock.advance(-1ns);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), ProviderErrorCode::invalid_argument);
  EXPECT_EQ(clock.now(), Instant{0ns});
}

TEST(ManualClockTest, WallIsSettable) {
  ManualClock clock;
  const std::chrono::system_clock::time_point wall{
      std::chrono::sys_days{std::chrono::year{2030} / 6 / 1}};
  clock.set_wall(wall);
  EXPECT_EQ(clock.wall_now(), wall);
}

TEST(RecordingDiagnosticsTest, SnapshotsInOrder) {
  RecordingDiagnostics diagnostics;
  diagnostics.report(Diagnostic{
      .severity = Severity::info, .category = "a", .message = "first"});
  diagnostics.report(Diagnostic{
      .severity = Severity::error, .category = "b", .message = "second"});
  const std::vector<Diagnostic> recorded = diagnostics.snapshot();
  ASSERT_EQ(recorded.size(), 2U);
  EXPECT_EQ(recorded[0].message, "first");
  EXPECT_EQ(recorded[1].severity, Severity::error);
}

TEST(MemoryAssetsTest, AddReadAndReplace) {
  MemoryAssets assets;
  const auto path = AssetPath::create("data/value.bin");
  ASSERT_TRUE(path.has_value());
  EXPECT_FALSE(assets.exists(*path));
  EXPECT_EQ(assets.read(*path).error().code(), ProviderErrorCode::not_found);
  const std::vector<std::byte> bytes{std::byte{7}, std::byte{8}};
  assets.add(*path, std::span<const std::byte>{bytes});
  ASSERT_TRUE(assets.exists(*path));
  EXPECT_TRUE(std::ranges::equal(assets.read(*path)->bytes(), bytes));
  assets.add(*path, std::string_view{"z"});
  EXPECT_EQ(assets.read(*path)->size(), 1U);
}

TEST(ScriptedLifecycleTest, DeliversPushedEventsInOrder) {
  ScriptedLifecycle lifecycle;
  lifecycle.push(LifecycleEvent::focus_lost);
  lifecycle.push(LifecycleEvent::exit_requested);
  EXPECT_EQ(lifecycle.poll(), LifecycleEvent::focus_lost);
  EXPECT_EQ(lifecycle.poll(), LifecycleEvent::exit_requested);
  EXPECT_EQ(lifecycle.poll(), std::nullopt);
  lifecycle.set_phase(ScenePhase::background);
  EXPECT_EQ(lifecycle.phase(), ScenePhase::background);
}

TEST(ScriptedLifecycleTest, HomeButtonBlocksCountDownToZero) {
  ScriptedLifecycle lifecycle;
  {
    auto outer = lifecycle.block_home_button();
    ASSERT_TRUE(outer.has_value());
    {
      const auto inner = lifecycle.block_home_button();
      ASSERT_TRUE(inner.has_value());
      EXPECT_EQ(lifecycle.live_home_button_blocks(), 2U);
    }
    EXPECT_EQ(lifecycle.live_home_button_blocks(), 1U);
    const HomeButtonBlock moved{std::move(*outer)};
    EXPECT_EQ(lifecycle.live_home_button_blocks(), 1U);
  }
  EXPECT_EQ(lifecycle.live_home_button_blocks(), 0U);
}

TEST(ScriptedLifecycleTest, BlockOutlivingProviderIsSafe) {
  std::optional<HomeButtonBlock> survivor;
  {
    ScriptedLifecycle lifecycle;
    auto block = lifecycle.block_home_button();
    ASSERT_TRUE(block.has_value());
    survivor.emplace(std::move(*block));
  }
  survivor.reset();
  SUCCEED();
}

TEST(ScriptedSystemInfoTest, ReturnsScriptedValues) {
  ScriptedSystemInfo system_info;
  EXPECT_EQ(system_info.battery()->percent.value(), 100);
  system_info.set_battery(
      BatteryState{.percent = BatteryPercent::of<42>(), .charging = true});
  system_info.set_network(NetworkState{.kind = NetworkKind::wifi,
                                       .signal_bars = SignalBars::of<3>(),
                                       .address = "192.168.0.2"});
  EXPECT_EQ(system_info.battery()->percent.value(), 42);
  EXPECT_TRUE(system_info.battery()->charging);
  EXPECT_EQ(system_info.network()->kind, NetworkKind::wifi);
  EXPECT_EQ(system_info.network()->address, "192.168.0.2");
  EXPECT_FALSE(SignalBars::create(4).has_value());
  EXPECT_FALSE(BatteryPercent::create(101).has_value());
}

TEST(ScriptedSystemInfoTest, DimmingInhibitsCountDownToZero) {
  ScriptedSystemInfo system_info;
  {
    const auto first = system_info.inhibit_dimming();
    const auto second = system_info.inhibit_dimming();
    ASSERT_TRUE(first.has_value() && second.has_value());
    EXPECT_EQ(system_info.live_dimming_inhibits(), 2U);
  }
  EXPECT_EQ(system_info.live_dimming_inhibits(), 0U);
}

TEST(MemoryFontsTest, RegisteredFacesReadBack) {
  MemoryFonts fonts;
  EXPECT_FALSE(fonts.available(SystemFont::korean));
  EXPECT_EQ(fonts.system_font(SystemFont::korean).error().code(),
            ProviderErrorCode::not_found);
  const std::vector<std::byte> face{std::byte{'O'}, std::byte{'T'},
                                    std::byte{'T'}, std::byte{'O'}};
  fonts.add(SystemFont::korean, face);
  ASSERT_TRUE(fonts.available(SystemFont::korean));
  EXPECT_TRUE(
      std::ranges::equal(fonts.system_font(SystemFont::korean)->bytes(), face));
}

TEST(MemoryFontsTest, ReplacedFacesStayValidForTheProviderLifetime) {
  MemoryFonts fonts;
  const std::vector<std::byte> first{std::byte{1}, std::byte{2}};
  const std::vector<std::byte> second{std::byte{3}};
  fonts.add(SystemFont::standard, first);
  const auto original = fonts.system_font(SystemFont::standard);
  ASSERT_TRUE(original.has_value());
  for (int round = 0; round < 64; ++round) {
    fonts.add(SystemFont::standard, second);
  }
  EXPECT_TRUE(std::ranges::equal(original->bytes(), first));
  EXPECT_EQ(fonts.system_font(SystemFont::standard)->size(), 1U);
}

TEST(RecordingProvidersTest, RecordHapticsSoundsClipboardAndBrowser) {
  RecordingHaptics haptics;
  RecordingAudio audio;
  MemoryClipboard clipboard;
  RecordingBrowser browser;
  ASSERT_TRUE(
      haptics.play(SensoryFeedback::impact(), HapticStrength::from_percent(150))
          .has_value());
  ASSERT_TRUE(audio.play(SoundRole::back).has_value());
  ASSERT_TRUE(audio.play(SoundRole::click).has_value());
  EXPECT_EQ(haptics.played(), std::vector{SensoryFeedback::impact()});
  EXPECT_EQ(haptics.strengths(),
            std::vector{HapticStrength::from_percent(150)});
  EXPECT_EQ(audio.played(), (std::vector{SoundRole::back, SoundRole::click}));
  EXPECT_EQ(*clipboard.read_text(), "");
  ASSERT_TRUE(clipboard.write_text("copied").has_value());
  EXPECT_EQ(*clipboard.read_text(), "copied");
  const auto url = Url::create("https://example.com");
  ASSERT_TRUE(url.has_value());
  ASSERT_TRUE(browser.open(*url).has_value());
  EXPECT_EQ(browser.opened(), std::vector{*url});
}

}
