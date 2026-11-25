#include <arisa/components/image.hpp>
#include <arisa/components/system_symbols.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/image_attributes.hpp>
#include <arisa/composition/image_environment.hpp>
#include <arisa/composition/symbol_effect.hpp>
#include <arisa/composition/symbol_variants.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/content_transition.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/theme/colour_value.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;

class SymbolParityTest : public FocusFixture {
 protected:
  using Builder = std::function<Element()>;

  class Scene final : public composition::Component {
   public:
    explicit Scene(const Builder* builder) noexcept : builder_(builder) {}

    auto operator==(const Scene&) const -> bool = default;

    auto body(composition::Context&) const -> Element override {
      return (*builder_)();
    }

   private:
    const Builder* builder_;
  };

  static auto glyph(std::string_view name, bool filled = false) -> std::string {
    const auto resolved = components::SystemSymbols::resolve(
        name, filled ? composition::SymbolVariants::fill
                     : composition::SymbolVariants::none);
    return resolved.has_value() ? resolved->base : std::string{};
  }

  static auto count(Application& application, std::string_view text)
      -> std::size_t {
    const std::vector<std::string> found = texts(application);
    return static_cast<std::size_t>(std::ranges::count(found, text));
  }

  static auto material(std::string_view name, bool filled) -> std::string {
    const auto point = components::SystemSymbols::codepoint(
        name, filled ? composition::SymbolVariants::fill
                     : composition::SymbolVariants::none);
    return point.has_value()
               ? components::SystemSymbols::text(
                     name, filled ? composition::SymbolVariants::fill
                                  : composition::SymbolVariants::none)
                     .value_or("")
               : std::string{};
  }
};

TEST_F(SymbolParityTest, SfNamesResolveThroughTheAliasTable) {
  const auto table = components::SystemSymbols::aliases();
  EXPECT_EQ(table.size(), components::SystemSymbols::alias_count);
  EXPECT_TRUE(std::ranges::is_sorted(table, {},
                                     &components::SystemSymbols::Alias::name));
  for (const components::SystemSymbols::Alias& alias : table) {
    EXPECT_TRUE(components::SystemSymbols::codepoint(alias.glyph).has_value())
        << alias.name << " -> " << alias.glyph;
    EXPECT_TRUE(components::SystemSymbols::known(alias.name)) << alias.name;
  }
  for (const components::SystemSymbols::Recipe& recipe :
       components::SystemSymbols::recipes()) {
    EXPECT_TRUE(components::SystemSymbols::alias(recipe.name).has_value())
        << recipe.name;
  }
  for (const components::SystemSymbols::Series& series :
       components::SystemSymbols::series()) {
    EXPECT_TRUE(components::SystemSymbols::alias(series.name).has_value())
        << series.name;
  }
  for (const std::string_view used :
       {"star.fill", "xmark", "checkmark", "square.and.arrow.up", "doc.on.doc",
        "gearshape", "magnifyingglass", "trash", "heart.fill", "wifi",
        "battery.100", "speaker.wave.3", "plus.circle.fill",
        "person.crop.circle"}) {
    EXPECT_FALSE(glyph(used).empty()) << used;
  }
  EXPECT_EQ(glyph("xmark"), material("close", false));
  EXPECT_EQ(glyph("star.fill"), material("star", true));
  EXPECT_EQ(glyph("star", true), material("star", true));
  EXPECT_EQ(glyph("chevron_right"), material("chevron_right", false));
  EXPECT_FALSE(components::SystemSymbols::resolve(
                   "not.a.symbol", composition::SymbolVariants::none)
                   .has_value());
}

TEST_F(SymbolParityTest, UnknownNamesDrawAPlaceholderAndLogOnce) {
  provider::headless::HeadlessProviderFactory providers;
  const Builder builder = [] {
    return components::VStack(
        components::Image::system_name("m30.missing.symbol"),
        components::Image::system_name("m30.missing.symbol"),
        components::Image::system_name("plus.circle.fill")
            .symbol_rendering_mode(composition::SymbolRenderingMode::palette));
  };
  Application application = start(providers, Element{Scene{&builder}});
  const auto logged = providers.diagnostics().snapshot();
  EXPECT_EQ(
      std::ranges::count_if(logged,
                            [](const provider::Diagnostic& entry) {
                              return entry.message.find("m30.missing.symbol") !=
                                     std::string::npos;
                            }),
      1);
  EXPECT_FALSE(
      std::ranges::any_of(logged, [](const provider::Diagnostic& entry) {
        return entry.message.find("plus.circle.fill") != std::string::npos;
      }));
}

TEST_F(SymbolParityTest, VariableValuesPickTheSeriesStepInEachMode) {
  provider::headless::HeadlessProviderFactory providers;
  const Builder builder = [] {
    return components::VStack(
        components::Image{components::Image::SystemName{"wifi"}, 0.5},
        components::Image{components::Image::SystemName{"wifi"}, 0.0}
            .symbol_variable_value_mode(
                composition::SymbolVariableValueMode::draw),
        components::Image{components::Image::SystemName{"battery.100"}, 1.0});
  };
  Application application = start(providers, Element{Scene{&builder}});
  EXPECT_EQ(count(application, material("network_wifi_2_bar", false)), 1U);
  EXPECT_EQ(count(application, material("signal_wifi_4_bar", false)), 1U);
  EXPECT_EQ(count(application, material("signal_wifi_0_bar", false)), 1U);
  EXPECT_EQ(count(application, material("battery_full", false)), 2U);
}

TEST_F(SymbolParityTest, RenderingModesLayerTheSymbol) {
  provider::headless::HeadlessProviderFactory providers;
  const Builder builder = [] {
    return components::VStack(
        components::Image::system_name("plus.circle.fill"),
        components::Image::system_name("plus.circle.fill")
            .symbol_rendering_mode(composition::SymbolRenderingMode::palette)
            .foreground_style(graphics::ShapeStyle{theme::ColourRole::red},
                              graphics::ShapeStyle{theme::ColourRole::blue}),
        components::Image::system_name("plus.circle.fill")
            .symbol_rendering_mode(
                composition::SymbolRenderingMode::hierarchical),
        components::Image::system_name("heart.fill")
            .symbol_rendering_mode(
                composition::SymbolRenderingMode::multicolor),
        components::Image::system_name("checkmark.circle.fill")
            .symbol_rendering_mode(composition::SymbolRenderingMode::multicolor)
            .symbol_color_rendering_mode(
                composition::SymbolColorRenderingMode::gradient));
  };
  Application application = start(providers, Element{Scene{&builder}});
  EXPECT_EQ(count(application, material("add_circle", true)), 3U);
  EXPECT_EQ(count(application, material("add", false)), 2U);
  EXPECT_EQ(count(application, material("favorite", true)), 1U);
  EXPECT_EQ(count(application, material("check_circle", true)), 2U);
  EXPECT_EQ(count(application, material("check", false)), 2U);
}

TEST_F(SymbolParityTest, VariableColorLightsOneLayerAtATime) {
  composition::SymbolEffect effect =
      composition::SymbolEffect::variable_color();
  EXPECT_FALSE(effect.runs_on_presentation());
  ASSERT_TRUE(effect.pose(0.3).layer_phase.has_value());
  EXPECT_NEAR(*effect.pose(0.3).layer_phase, 0.3 / effect.cycle(), 1e-9);
  provider::headless::HeadlessProviderFactory providers;
  const Builder builder = [] {
    return components::VStack(
        components::Image::system_name("wifi").symbol_effect(
            composition::SymbolEffect::variable_color(), true),
        components::Image::system_name("plus.circle.fill")
            .symbol_rendering_mode(composition::SymbolRenderingMode::palette)
            .symbol_effect(composition::SymbolEffect::variable_color(), true));
  };
  Application application = start(providers, Element{Scene{&builder}});
  std::vector<std::string> seen;
  for (int step = 0; step < 8; ++step) {
    ASSERT_TRUE(
        providers.clock().advance(std::chrono::milliseconds{150}).has_value());
    frames(application, 1);
    for (const std::string_view name :
         {"signal_wifi_0_bar", "network_wifi_1_bar", "network_wifi_2_bar",
          "network_wifi_3_bar"}) {
      if (count(application, material(name, false)) > 0) {
        seen.emplace_back(name);
      }
    }
  }
  std::ranges::sort(seen);
  const auto [first, last] = std::ranges::unique(seen);
  seen.erase(first, last);
  EXPECT_GE(seen.size(), 3U);
}

TEST_F(SymbolParityTest, SymbolEffectsRemovedStopsEveryEffect) {
  provider::headless::HeadlessProviderFactory providers;
  const Builder builder = [] {
    return components::VStack(
        components::Image::system_name("wifi")
            .symbol_effect(composition::SymbolEffect::variable_color(), true)
            .symbol_effect(composition::SymbolEffect::pulse(), true)
            .symbol_effects_removed());
  };
  Application application = start(providers, Element{Scene{&builder}});
  for (int step = 0; step < 4; ++step) {
    ASSERT_TRUE(
        providers.clock().advance(std::chrono::milliseconds{150}).has_value());
    frames(application, 1);
    EXPECT_EQ(count(application, material("wifi", false)), 1U);
    EXPECT_EQ(count(application, material("signal_wifi_4_bar", false)), 0U);
  }
}

TEST_F(SymbolParityTest, WiggleTakesACustomAngleAndReplaceIsATransition) {
  const composition::SymbolEffect plain = composition::SymbolEffect::wiggle();
  const composition::SymbolEffect wide =
      plain.custom(motion::Angle::degrees(30.0));
  EXPECT_NEAR(wide.wiggle_angle().radians(), std::numbers::pi / 6.0, 1e-9);
  EXPECT_NEAR(plain.wiggle_angle().radians(),
              composition::SymbolEffect::default_wiggle_radians, 1e-9);
  const double at = plain.cycle() / 12.0;
  EXPECT_NEAR(wide.pose(at).rotation / plain.pose(at).rotation,
              (std::numbers::pi / 6.0) /
                  composition::SymbolEffect::default_wiggle_radians,
              1e-6);
  EXPECT_FALSE(wide == plain);
  using Transition = motion::ContentTransition;
  EXPECT_EQ(Transition::symbol_effect.kind(), Transition::Kind::symbol_effect);
  EXPECT_TRUE(Transition::symbol_effect.moves());
  const Transition faster =
      Transition::symbol_effect_replace(Transition::SymbolReplace::up_up, 2.0);
  EXPECT_EQ(faster.replace(), Transition::SymbolReplace::up_up);
  EXPECT_DOUBLE_EQ(faster.speed(), 2.0);
  EXPECT_FALSE(faster == Transition::symbol_effect);
}

}
