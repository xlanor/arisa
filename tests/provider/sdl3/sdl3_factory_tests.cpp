#include <arisa/conformance/conformance_suite.hpp>
#include <arisa/provider/headless/memory_assets.hpp>
#include <arisa/provider/headless/recording_diagnostics.hpp>
#include <arisa/provider/headless/recording_haptics.hpp>
#include <arisa/provider/sdl3/sdl3_factory.hpp>
#include <arisa/provider/standard/directory_assets.hpp>

#include "sdl3_test.hpp"

#include <SDL3/SDL_events.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <print>
#include <string_view>
#include <utility>

namespace arisa::provider::sdl3 {

class Sdl3ProviderFactoryTest : public Sdl3Test {
 protected:
  static void expect_passes(const conformance::Report& report) {
    for (const conformance::CheckResult& result : report) {
      if (!result.passed) {
        std::println("FAILED {}: {}", result.name, result.detail);
      }
    }
    EXPECT_TRUE(report.passed());
    EXPECT_GT(report.size(), 0U);
  }

  static auto has_check(const conformance::Report& report,
                        std::string_view name) -> bool {
    return std::ranges::any_of(report,
                               [name](const conformance::CheckResult& result) {
                                 return result.name == name;
                               });
  }

  static auto unavailable(const headless::RecordingDiagnostics& diagnostics,
                          std::string_view capability) -> std::ptrdiff_t {
    return std::ranges::count_if(
        diagnostics.snapshot(), [capability](const Diagnostic& diagnostic) {
          return diagnostic.category == "arisa.sdl3" &&
                 diagnostic.message.starts_with(capability);
        });
  }

  standard::DirectoryAssets fixtures_{ARISA_TEST_FIXTURES};
};

TEST_F(Sdl3ProviderFactoryTest, SuppliesTheDesktopCapabilities) {
  headless::RecordingDiagnostics diagnostics;
  auto factory = Sdl3ProviderFactory::create(
      Providers{.diagnostics = diagnostics, .assets = fixtures_});
  ASSERT_TRUE(factory.has_value());
  const ProviderInfo info = factory->info();
  EXPECT_EQ(info.name, "arisa sdl3");
  EXPECT_EQ(info.abi_version, abi_version);
  EXPECT_TRUE(factory->supplies(Capability::required));
  EXPECT_TRUE(factory->supplies(Capability::lifecycle));
  EXPECT_TRUE(factory->supplies(Capability::system_info));
  EXPECT_TRUE(factory->supplies(Capability::audio));
  EXPECT_TRUE(factory->supplies(Capability::browser));
  EXPECT_FALSE(factory->supplies(Capability::fonts));
  EXPECT_EQ(unavailable(diagnostics, "fonts"), 1);
  EXPECT_EQ(&factory->assets(), &fixtures_);
  EXPECT_EQ(&factory->diagnostics(), &diagnostics);
}

TEST_F(Sdl3ProviderFactoryTest, SuppliesFontsWhenTheStandardFaceExists) {
  headless::MemoryAssets assets;
  static constexpr std::array face{std::byte{'t'}, std::byte{'r'},
                                   std::byte{'u'}, std::byte{'e'}};
  assets.add(*AssetPath::create("arisa/fonts/standard.ttf"), face);
  auto factory = Sdl3ProviderFactory::create(Providers{.assets = assets});
  ASSERT_TRUE(factory.has_value());
  EXPECT_TRUE(factory->supplies(Capability::fonts));
  EXPECT_TRUE(factory->fonts().available(SystemFont::standard));
}

TEST_F(Sdl3ProviderFactoryTest, OverridesReplaceStockProviders) {
  headless::RecordingHaptics haptics;
  auto factory = Sdl3ProviderFactory::create(
      Providers{.assets = fixtures_, .haptics = haptics});
  ASSERT_TRUE(factory.has_value());
  EXPECT_EQ(&factory->haptics(), &haptics);
  EXPECT_TRUE(factory->supplies(Capability::haptics));
}

TEST_F(Sdl3ProviderFactoryTest, DefaultAssetsLiveBesideTheExecutable) {
  EXPECT_EQ(Sdl3ProviderFactory::default_assets_root().filename(),
            Sdl3ProviderFactory::assets_folder);
}

TEST_F(Sdl3ProviderFactoryTest, MovingKeepsEventsFlowing) {
  auto created = Sdl3ProviderFactory::create(Providers{.assets = fixtures_});
  ASSERT_TRUE(created.has_value());
  Sdl3ProviderFactory factory{std::move(*created)};
  while (factory.lifecycle().poll().has_value()) {
  }
  SDL_Event quit{};
  quit.type = SDL_EVENT_QUIT;
  ASSERT_TRUE(SDL_PushEvent(&quit));
  EXPECT_EQ(factory.lifecycle().poll(), LifecycleEvent::exit_requested);
  EXPECT_TRUE(factory.audio().play(SoundRole::click).has_value());
}

TEST_F(Sdl3ProviderFactoryTest, PassesConformance) {
  auto factory = Sdl3ProviderFactory::create(Providers{.assets = fixtures_});
  ASSERT_TRUE(factory.has_value());
  const conformance::Report report =
      conformance::ConformanceSuite::run(*factory);
  expect_passes(report);
  EXPECT_TRUE(has_check(report, "threading.self_destruction"));
  EXPECT_TRUE(has_check(report, "lifecycle.home_button_block"));
  EXPECT_TRUE(has_check(report, "system_info.dimming_inhibit"));
  EXPECT_TRUE(has_check(report, "audio.play"));
  EXPECT_TRUE(has_check(report, "fonts.absent"));
  EXPECT_TRUE(has_check(report, "browser.url_rules"));
  EXPECT_FALSE(has_check(report, "browser.open"));
  EXPECT_TRUE(has_check(report, "clipboard.round_trip") ||
              has_check(report, "clipboard.absent"));
}

}
