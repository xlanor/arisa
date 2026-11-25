#include <arisa/appearance/appearance_model.hpp>
#include <arisa/appearance/theme_vocabulary.hpp>
#include <arisa/appearance/theme_watcher.hpp>
#include <arisa/async/manual_executor.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/memory_storage.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <format>
#include <optional>
#include <print>
#include <string>
#include <string_view>

namespace arisa::runtime {

class ThemeReloadTest : public GalleryFixture {
 protected:
  static constexpr std::chrono::milliseconds interval{250};
  static constexpr std::string_view swatch_key = "reload.swatch";

  class Swatch final : public composition::Component {
   public:
    auto operator==(const Swatch&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const theme::Theme& theme =
          context.environment(components::Environment::theme);
      return composition::Element{
          components::Text(
              std::format("{} {:g}",
                          appearance::ThemeVocabulary::hex(
                              theme.palette(theme::ColourRole::accent).light),
                          theme.radii().card))}
          .id(std::string{swatch_key});
    }
  };

  static constexpr std::string_view base_file =
      "schema = 1\nname = \"base\"\n[colours]\naccent = \"#112233\"\n";

  static auto live_file(int card) -> std::string {
    return std::format(
        "schema = 1\nname = \"live\"\nextends = \"base\"\n[radii]\ncard = "
        "{}\n",
        card);
  }

  void write(std::string_view file, std::string_view text) {
    auto path = provider::AssetPath::create(std::format("themes/{}", file));
    ASSERT_TRUE(path.has_value());
    ASSERT_TRUE(
        storage_.write(*path, std::as_bytes(std::span{text})).has_value());
  }

  auto start(provider::headless::HeadlessProviderFactory& providers,
             std::optional<std::chrono::milliseconds> reload) -> Application {
    load_fonts(providers);
    write("base.toml", base_file);
    write("live.toml", live_file(10));
    appearance::AppearancePolicy policy;
    policy.defaults.theme = "live";
    auto application = Application::create(
        providers, Swatch{},
        ApplicationOptions{
            .executors = async::Executors{.background = background_,
                                          .blocking = background_},
            .fonts = examples::GalleryFonts::load(providers.assets()),
            .atlas = {},
            .interaction = {},
            .appearance = AppearanceOptions{.policy = std::move(policy),
                                            .storage = std::ref(storage_),
                                            .themes = std::nullopt,
                                            .hot_reload = reload}});
    EXPECT_TRUE(application.has_value());
    Application started = std::move(*application);
    pump(providers, started, 4);
    return started;
  }

  void pump(provider::headless::HeadlessProviderFactory& providers,
            Application& application, int count) {
    for (int index = 0; index < count; ++index) {
      ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
      ASSERT_TRUE(application.frame().has_value());
      background_.drain();
    }
  }

  static auto shown(Application& application) -> std::string {
    const composition::Tree& tree = application.root().tree();
    const auto found =
        tree.find(composition::ElementKey{std::string{swatch_key}});
    if (!found.has_value()) {
      return {};
    }
    const auto text =
        tree.node(*found)->get().content_as<components::Text::Content>();
    return text.has_value() ? std::string{text->get().text()} : std::string{};
  }

  auto until_shown(provider::headless::HeadlessProviderFactory& providers,
                   Application& application, std::string_view wanted, int limit)
      -> int {
    for (int frames = 0; frames < limit; ++frames) {
      if (shown(application) == wanted) {
        return frames;
      }
      pump(providers, application, 1);
    }
    return shown(application) == wanted ? limit : -1;
  }

  async::ManualExecutor background_;
  provider::headless::MemoryStorage storage_;
};

TEST_F(ThemeReloadTest, AChangedThemeFileAppliesWithinThePollingInterval) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, interval);
  ASSERT_EQ(shown(application), "#112233 10");
  ASSERT_TRUE(application.theme_watcher().has_value());
  pump(providers, application, 20);
  ASSERT_GT(application.theme_watcher()->get().polls(), 0U);
  write("live.toml", live_file(20));
  const auto written = providers.clock().now().since_origin();
  const int frames = until_shown(providers, application, "#112233 20", 40);
  ASSERT_GE(frames, 0);
  const auto elapsed = providers.clock().now().since_origin() - written;
  EXPECT_LE(elapsed, interval + (3 * frame_step));
  EXPECT_EQ(application.theme_watcher()->get().reloads(), 1U);
  EXPECT_EQ(application.appearance()->theme().radii().card, 20.0F);
  std::println(
      "reload applied {} ms after the write ({} frames)",
      std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(),
      frames);
}

TEST_F(ThemeReloadTest, ABrokenFileKeepsTheOldThemeAndReportsWhere) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, interval);
  pump(providers, application, 20);
  static_cast<void>(providers.diagnostics().take());
  write("live.toml",
        "schema = 1\nextends = \"base\"\n[radii]\ncard = \"wide\"\n");
  pump(providers, application, 30);
  EXPECT_EQ(shown(application), "#112233 10");
  EXPECT_EQ(application.theme_watcher()->get().failures(), 1U);
  EXPECT_EQ(application.theme_watcher()->get().reloads(), 0U);
  const auto& error = application.appearance()->load_error();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(error->file(), "themes/live.toml");
  EXPECT_EQ(error->line(), 4U);
  EXPECT_EQ(error->key(), "radii.card");
  const std::vector<provider::Diagnostic> reported =
      providers.diagnostics().take();
  EXPECT_TRUE(std::ranges::any_of(reported, [](const auto& diagnostic) {
    return diagnostic.category == "theme" &&
           diagnostic.message.contains("themes/live.toml:4: radii.card");
  }));
  write("live.toml", live_file(30));
  EXPECT_GE(until_shown(providers, application, "#112233 30", 40), 0);
  EXPECT_FALSE(application.appearance()->load_error().has_value());
}

TEST_F(ThemeReloadTest, AChangeToAParentInTheExtendsChainApplies) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, interval);
  pump(providers, application, 20);
  EXPECT_EQ(application.theme_watcher()->get().watching().size(), 4U);
  write("base.toml",
        "schema = 1\nname = \"base\"\n[colours]\naccent = \"#445566\"\n");
  EXPECT_GE(until_shown(providers, application, "#445566 10", 40), 0);
  EXPECT_EQ(application.theme_watcher()->get().reloads(), 1U);
}

TEST_F(ThemeReloadTest, DisabledReloadNeverPolls) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, std::nullopt);
  const std::size_t stamped = storage_.stamps();
  pump(providers, application, 120);
  write("live.toml", live_file(20));
  pump(providers, application, 120);
  EXPECT_FALSE(application.theme_watcher().has_value());
  EXPECT_EQ(storage_.stamps(), stamped);
  EXPECT_EQ(storage_.stamps(), 0U);
  EXPECT_EQ(background_.pending(), 0U);
  EXPECT_EQ(shown(application), "#112233 10");
}

TEST_F(ThemeReloadTest, FiftyReloadsDoNotGrowLiveAllocations) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, interval);
  pump(providers, application, 20);
  std::optional<std::int64_t> settled;
  for (int round = 1; round <= 50; ++round) {
    const int card = 20 + (round % 2);
    write("live.toml", live_file(card));
    ASSERT_GE(until_shown(providers, application,
                          std::format("#112233 {}", card), 40),
              0)
        << "round " << round;
    pump(providers, application, 30);
    if (round == 10) {
      settled = provider::AllocationMeter::live();
    }
  }
  EXPECT_EQ(application.theme_watcher()->get().reloads(), 50U);
  const std::optional<std::int64_t> finished =
      provider::AllocationMeter::live();
  if (settled.has_value() && finished.has_value()) {
    EXPECT_LE(*finished - *settled, 0) << "live allocations grew";
    std::println("50 reloads: live allocations {} after 10, {} after 50",
                 *settled, *finished);
  }
}

}
