#include <arisa/components/button.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/appearance_settings_section.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/oklch.hpp>

#include <controls_page.hpp>
#include <fidelity_page.hpp>
#include <gallery_fixture.hpp>
#include <panels_page.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::runtime {

class GalleryAccentTest
    : public GalleryFixture,
      public ::testing::WithParamInterface<examples::GalleryPage> {
 public:
  static auto detects(const composition::Node& button) -> bool {
    return prominent(button);
  }

 protected:
  static auto accented(const composition::Node& node) -> bool {
    const auto& background = node.modifiers().background;
    if (!background.has_value()) {
      return false;
    }
    const std::optional<theme::ColourValue> fill = background->fill.colour();
    const theme::ColourRole* role = fill.has_value() ? fill->role() : nullptr;
    return role != nullptr && (*role == theme::ColourRole::accent ||
                               *role == theme::ColourRole::accent_pressed);
  }

  static auto prominent(const composition::Node& button) -> bool {
    const composition::Node* at = &button;
    for (int depth = 0; depth < 8; ++depth) {
      if (accented(*at)) {
        return true;
      }
      if (at->modifiers().background.has_value()) {
        return false;
      }
      std::vector<const composition::Node*> live;
      for (const composition::Node& child : at->children()) {
        if (child.phase() == composition::NodeLifecyclePhase::live) {
          live.push_back(&child);
        }
      }
      if (live.size() != 1) {
        return false;
      }
      at = live.front();
    }
    return false;
  }

  static auto inside(const composition::Tree& tree,
                     const composition::Node& node, std::string_view key)
      -> bool {
    const auto boundary = tree.find(composition::ElementKey{key});
    if (!boundary.has_value()) {
      return false;
    }
    std::optional<composition::NodeId> at = node.id();
    while (at.has_value()) {
      if (*at == *boundary) {
        return true;
      }
      at = tree.node(*at)->get().parent();
    }
    return false;
  }

  static auto offenders(Application& application) -> std::vector<std::string> {
    const composition::Tree& tree = application.root().tree();
    std::vector<std::string> found;
    walk(tree.root(), [&](const composition::Node& node) {
      const auto button = node.content_as<components::Button::Content>();
      if (!button.has_value() || !prominent(node) ||
          inside(tree, node, examples::ControlsPage::style_demos) ||
          inside(tree, node, examples::ControlsPage::role_demos) ||
          inside(tree, node,
                 examples::FidelityPage::key("button.bordered_prominent")) ||
          inside(tree, node, examples::PanelsPage::tiles_key)) {
        return;
      }
      found.push_back(std::string{button->get().label()});
    });
    return found;
  }
};

TEST_P(GalleryAccentTest, OnlySelectionAndStateControlsUseAccentFills) {
  for (const theme::ColorScheme scheme :
       {theme::ColorScheme::light, theme::ColorScheme::dark}) {
    provider::headless::HeadlessProviderFactory providers;
    Application application = start_gallery(providers, GetParam(), scheme);
    run(providers, application, 20);
    const std::vector<std::string> found = offenders(application);
    EXPECT_TRUE(found.empty())
        << examples::GalleryPages::name(GetParam()) << " has " << found.size()
        << " accent-filled buttons, first: "
        << (found.empty() ? std::string{} : found.front());
  }
}

TEST_F(GalleryFixture, AccentDetectorSeesTheControlsStyleDemos) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::controls);
  run(providers, application, 20);
  std::size_t seen = 0;
  walk(application.root().tree().root(),
       [&seen](const composition::Node& node) {
         if (node.content_as<components::Button::Content>().has_value() &&
             GalleryAccentTest::detects(node)) {
           ++seen;
         }
       });
  EXPECT_GE(seen, 2U);
}

class GalleryAccentFamilyTest : public GalleryFixture,
                                public ::testing::WithParamInterface<bool> {
 protected:
  static auto packed(theme::Colour colour) -> std::uint32_t {
    const auto byte = [](float value) {
      return static_cast<std::uint32_t>(
          std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
    };
    return byte(colour.red) | (byte(colour.green) << 8U) |
           (byte(colour.blue) << 16U);
  }

  static auto drawn(provider::headless::HeadlessProviderFactory& providers,
                    provider::ShapeKind kind, theme::Colour colour) -> bool {
    const std::uint32_t wanted = packed(colour);
    return std::ranges::any_of(
        providers.gpu().last_shapes().vertices,
        [&](const provider::ShapeVertex& vertex) {
          if (vertex.kind != kind || (vertex.colour >> 24U) < 0x80U) {
            return false;
          }
          for (const std::uint32_t shift : {0U, 8U, 16U}) {
            const auto have =
                static_cast<int>((vertex.colour >> shift) & 0xFFU);
            const auto want = static_cast<int>((wanted >> shift) & 0xFFU);
            if (std::abs(have - want) > 2) {
              return false;
            }
          }
          return true;
        });
  }

  static auto hue_gap(theme::Colour left, theme::Colour right) -> float {
    const float gap = std::fmod(std::abs(theme::Oklch::from(left).hue() -
                                         theme::Oklch::from(right).hue()),
                                360.0F);
    return std::min(gap, 360.0F - gap);
  }
};

TEST_P(GalleryAccentFamilyTest, EveryAccentRecoloursTheDerivedRoles) {
  for (const theme::ColorScheme scheme :
       {theme::ColorScheme::light, theme::ColorScheme::dark}) {
    provider::headless::HeadlessProviderFactory providers;
    AppearanceOptions options{.policy = examples::GalleryScreen::policy(scheme),
                              .storage = {}};
    if (GetParam()) {
      options.policy.defaults.theme = "arisa-theme-glass";
    }
    Application application = start_gallery(
        providers, examples::GalleryPage::controls, scheme,
        provider::headless::ScriptedSurface::default_size, std::move(options));
    ASSERT_TRUE(application.appearance().has_value());
    const appearance::AppearanceModel model = *application.appearance();
    EXPECT_EQ(
        model.theme().colour(theme::ColourRole::accent, scheme),
        theme::Theme::defaults().colour(theme::ColourRole::accent, scheme));
    press(providers, application, provider::Button::dpad_right);
    run(providers, application, 10);
    press(providers, application, provider::Button::dpad_down);
    for (const structure::AppearanceSettingsSection::Accent& accent :
         structure::AppearanceSettingsSection::accents) {
      model.update([&accent](appearance::AppearanceOverrides& overrides) {
        overrides.accent = accent.colour;
      });
      run(providers, application, 30);
      const theme::Theme& theme = model.theme();
      const theme::Colour tint =
          theme.colour(theme::ColourRole::accent, scheme);
      const bool chromatic = theme::Oklch::from(accent.colour).chroma() > 0.05F;
      for (const theme::ColourRole role :
           {theme::ColourRole::accent, theme::ColourRole::accent_pressed,
            theme::ColourRole::keyboard_focus_indicator,
            theme::ColourRole::selected_content_background,
            theme::ColourRole::selected_control,
            theme::ColourRole::selected_text_background}) {
        const theme::Colour resolved = theme.colour(role, scheme);
        if (chromatic) {
          EXPECT_LT(hue_gap(resolved, accent.colour), 20.0F)
              << accent.name << " role " << static_cast<int>(role);
        }
      }
      const float ring =
          theme::Oklch::from(
              theme.colour(theme::ColourRole::focus_ring, scheme))
              .lightness();
      const float window =
          theme::Oklch::from(
              theme.colour(theme::ColourRole::window_background, scheme))
              .lightness();
      EXPECT_GE(std::abs(ring - window), 0.2F) << accent.name;
      EXPECT_TRUE(drawn(providers, provider::ShapeKind::fill, tint))
          << accent.name;
      EXPECT_TRUE(drawn(providers, provider::ShapeKind::stroke,
                        theme.colour(theme::ColourRole::focus_ring, scheme)))
          << accent.name;
      EXPECT_TRUE(
          drawn(providers, provider::ShapeKind::fill,
                theme.colour(theme::ColourRole::selected_control, scheme)) ||
          drawn(providers, provider::ShapeKind::fill,
                theme.colour(theme::ColourRole::selected_content_background,
                             scheme)))
          << accent.name;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(Packs, GalleryAccentFamilyTest, ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& shown) {
                           return std::string{shown.param ? "glass"
                                                          : "default"};
                         });

INSTANTIATE_TEST_SUITE_P(
    Pages, GalleryAccentTest, ::testing::ValuesIn(examples::GalleryPages::all),
    [](const ::testing::TestParamInfo<examples::GalleryPage>& page) {
      return std::string{examples::GalleryPages::name(page.param)};
    });

}
