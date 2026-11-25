#include <arisa/components/accessibility_tree.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>

#include <gallery_fixture.hpp>
#include <world_page.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::runtime {

class GalleryWorldTest : public GalleryFixture {
 protected:
  using Page = examples::WorldPage;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"world-host", true};

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return Page{context.state(shown)};
    }
  };

  static void strings(provider::headless::HeadlessProviderFactory& providers) {
    for (const std::string_view tag : {"en", "ja", "ar", "he", "fr", "de"}) {
      for (const std::string_view folder :
           {std::string_view{"arisa/strings"},
            std::string_view{"gallery/strings"}}) {
        const std::string file = std::format("{}/{}.toml", folder, tag);
        std::ifstream stream{std::filesystem::path{ARISA_SOURCE_ASSETS} / file,
                             std::ios::binary};
        if (!stream) {
          continue;
        }
        const std::string text{std::istreambuf_iterator<char>{stream},
                               std::istreambuf_iterator<char>{}};
        providers.assets().add(*provider::AssetPath::create(file),
                               std::string_view{text});
      }
    }
  }

  static auto open(provider::headless::HeadlessProviderFactory& providers,
                   composition::Element content, std::string_view language)
      -> Application {
    load_fonts(providers);
    strings(providers);
    providers.system_info().set_languages({std::string{language}});
    providers.surface().resize(
        provider::SurfaceSize{.width = provider::SurfaceWidth{1280},
                              .height = provider::SurfaceHeight{720}});
    providers.surface().set_display_mode(provider::DisplayMode::handheld);
    auto created = Application::create(
        providers, std::move(content),
        ApplicationOptions{
            .executors = std::nullopt,
            .fonts = examples::GalleryFonts::load(providers.assets()),
            .atlas = {},
            .interaction = {},
            .localization = LocalizationOptions{
                .locale = std::nullopt,
                .strings = std::string{examples::GalleryScreen::strings}}});
    EXPECT_TRUE(created.has_value());
    Application application = std::move(*created);
    run(providers, application, 20);
    return application;
  }

  static auto spoken(Application& application, std::string_view key)
      -> std::string {
    const auto node =
        application.root().tree().find(composition::ElementKey{key});
    if (!node.has_value()) {
      ADD_FAILURE() << "no node keyed " << key;
      return {};
    }
    const auto element =
        components::AccessibilityTree{application.root().tree()}.element(*node);
    return element.has_value() ? std::string{element->label()} : std::string{};
  }

  static auto labels(Application& application) -> std::vector<std::string> {
    std::vector<std::string> found;
    for (const components::AccessibilityElement& each :
         components::AccessibilityTree{application.root().tree()}.elements()) {
      found.emplace_back(each.label());
    }
    return found;
  }

  static auto x_of(Application& application, std::string_view key) -> float {
    const auto node =
        application.root().tree().find(composition::ElementKey{key});
    if (!node.has_value()) {
      return -1.0F;
    }
    const auto found = application.root().tree().node(*node);
    return found.has_value() ? found->get().frame().x : -1.0F;
  }
};

TEST_F(GalleryWorldTest, FormatsFollowTheSystemLanguage) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, Host{}, "de-DE");
  EXPECT_EQ(spoken(application, Page::number_view), "Zahl, 1.234.567,891");
  EXPECT_EQ(spoken(application, Page::count_key(1)), "1 Datei");
  EXPECT_EQ(spoken(application, Page::count_key(5)), "5 Dateien");
}

TEST_F(GalleryWorldTest, ArabicUsesEveryPluralForm) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, Host{}, "ar");
  EXPECT_EQ(spoken(application, Page::count_key(0)), "لا ملفات");
  EXPECT_EQ(spoken(application, Page::count_key(1)), "ملف واحد");
  EXPECT_EQ(spoken(application, Page::count_key(2)), "ملفان");
  EXPECT_EQ(spoken(application, Page::count_key(5)), "5 ملفات");
  EXPECT_EQ(spoken(application, Page::count_key(11)), "11 ملفًا");
}

TEST_F(GalleryWorldTest, RightToLeftMirrorsRowsButNotTheLeftToRightOne) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, Host{}, "he");
  EXPECT_GT(x_of(application, Page::mirrored_row), 600.0F);
  EXPECT_LT(x_of(application, Page::fixed_first),
            x_of(application, Page::fixed_last));
  EXPECT_GE(x_of(application, Page::fixed_first), 0.0F);
}

TEST_F(GalleryWorldTest, AccessibilityMetadataIsPublished) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, Host{}, "en");
  const components::AccessibilityTree tree{application.root().tree()};
  const auto described = tree.find(Page::described_button);
  ASSERT_TRUE(described.has_value());
  EXPECT_EQ(described->label(), "Globe");
  EXPECT_EQ(described->hint(), "Opens the language list");
  const auto combined = tree.find(Page::combined_row);
  ASSERT_TRUE(combined.has_value());
  EXPECT_EQ(combined->label(), "language, 12, Files");
  for (const std::string& label : labels(application)) {
    EXPECT_NE(label, "star");
  }
}

TEST_F(GalleryWorldTest, SettledPageLaysOutAndAllocatesNothing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, Host{}, "ja");
  run(providers, application, 10);
  const std::optional<std::uint64_t> before =
      provider::AllocationMeter::count();
  run(providers, application, 10);
  const std::optional<std::uint64_t> after = provider::AllocationMeter::count();
  if (before.has_value() && after.has_value()) {
    EXPECT_EQ(*after - *before, 0U);
  }
  EXPECT_EQ(application.last_frame().evaluations, 0U);
}

TEST_F(GalleryWorldTest, TheGalleryChromeMirrorsAndTranslates) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(
      providers,
      examples::GalleryScreen{examples::GalleryPage::settings, theme::Seed{}},
      "ar");
  const std::vector<std::string> shown = labels(application);
  EXPECT_NE(std::ranges::find(shown, "الإعدادات"), shown.end());
  const float scheme =
      x_of(application, examples::GalleryScreen::scheme_button);
  EXPECT_GE(scheme, 0.0F);
  EXPECT_LT(scheme, 640.0F);
}

}
