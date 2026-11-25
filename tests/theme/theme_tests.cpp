#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/theme/adaptive.hpp>
#include <arisa/theme/appearance_variants.hpp>
#include <arisa/theme/color.hpp>
#include <arisa/theme/oklab.hpp>
#include <arisa/theme/oklch.hpp>
#include <arisa/theme/theme.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>

namespace arisa::theme {

class ThemeTest : public ::testing::Test {
 protected:
  static constexpr float channel = 1.0F / 255.0F;

  static void expect_colour(Colour actual, Colour expected,
                            float tolerance = channel) {
    EXPECT_NEAR(actual.red, expected.red, tolerance);
    EXPECT_NEAR(actual.green, expected.green, tolerance);
    EXPECT_NEAR(actual.blue, expected.blue, tolerance);
    EXPECT_NEAR(actual.alpha, expected.alpha, tolerance);
  }

  class Reader final : public composition::Component {
   public:
    explicit Reader(std::shared_ptr<int> reads) : reads_(std::move(reads)) {}

    auto operator==(const Reader&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      ++*reads_;
      const std::optional<ColorScheme> scheme =
          context.environment(components::Environment::colour_scheme);
      const Theme& theme = context.environment(components::Environment::theme);
      return components::Text(scheme == ColorScheme::dark &&
                                      theme.seed().accent.red > 0.5F
                                  ? "dark warm"
                              : scheme == ColorScheme::dark ? "dark"
                                                            : "light");
    }

   private:
    std::shared_ptr<int> reads_;
  };

  class Switcher final : public composition::Component {
   public:
    Switcher(std::shared_ptr<composition::Observable<bool>> dark,
             std::shared_ptr<composition::Observable<bool>> warm,
             std::shared_ptr<int> reads)
        : dark_(std::move(dark)),
          warm_(std::move(warm)),
          reads_(std::move(reads)) {}

    auto operator==(const Switcher&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const bool dark = context.read(*dark_);
      const bool warm = context.read(*warm_);
      return components::VStack(
                 composition::Element{Reader{reads_}}.id("reader"))
          .environment(components::Environment::colour_scheme,
                       dark ? ColorScheme::dark : ColorScheme::light)
          .environment(components::Environment::theme,
                       Theme::from(Seed{.accent = warm ? Colour::hex(0xFF9500)
                                                       : Colour::hex(0x007AFF),
                                        .scheme = ColorScheme::light}));
    }

   private:
    std::shared_ptr<composition::Observable<bool>> dark_;
    std::shared_ptr<composition::Observable<bool>> warm_;
    std::shared_ptr<int> reads_;
  };

  static auto label(const composition::Tree& tree) -> std::string_view {
    const auto reader = tree.find(composition::ElementKey{"reader"});
    const composition::Node& node = tree.node(*reader)->get();
    return node.children()
        .front()
        .content_as<components::Text::Content>()
        ->get()
        .text();
  }
};

TEST_F(ThemeTest, OklchMatchesTheReferenceConversion) {
  const Oklch red = Oklch::from(Colour::hex(0xFF0000));
  EXPECT_NEAR(red.lightness(), 0.62796F, 1.0e-4F);
  EXPECT_NEAR(red.chroma(), 0.25768F, 1.0e-4F);
  EXPECT_NEAR(red.hue(), 29.2339F, 1.0e-2F);
  const Oklch white = Oklch::from(Colour::hex(0xFFFFFF));
  EXPECT_NEAR(white.lightness(), 1.0F, 1.0e-4F);
  EXPECT_NEAR(white.chroma(), 0.0F, 1.0e-4F);
  const Oklch accent = Oklch::from(Colour::hex(0x5B8CFF));
  EXPECT_NEAR(accent.lightness(), 0.66228F, 1.0e-4F);
  EXPECT_NEAR(accent.chroma(), 0.17859F, 1.0e-4F);
  EXPECT_NEAR(accent.hue(), 264.6445F, 1.0e-2F);
  expect_colour(accent.to_colour(), Colour::hex(0x5B8CFF));
}

TEST_F(ThemeTest, OutOfGamutColoursReduceChromaAndKeepLightness) {
  const Oklch vivid{0.7F, 0.4F, 150.0F};
  EXPECT_FALSE(vivid.in_gamut());
  const Oklch mapped = Oklch::from(vivid.to_colour());
  EXPECT_NEAR(mapped.lightness(), 0.7F, 2.0e-3F);
  EXPECT_LT(mapped.chroma(), 0.4F);
  EXPECT_NEAR(mapped.hue(), 150.0F, 1.0F);
}

TEST_F(ThemeTest, SeedingDerivesTheAccentFamilyInOklch) {
  const Colour seed = Colour::hex(0x5B8CFF);
  const Theme theme =
      Theme::from(Seed{.accent = seed, .scheme = ColorScheme::light});
  const Oklch base = Oklch::from(seed);
  expect_colour(theme.palette(ColourRole::accent).light, seed);
  expect_colour(theme.palette(ColourRole::accent).dark, seed);
  const Oklch pressed =
      Oklch::from(theme.palette(ColourRole::accent_pressed).light);
  EXPECT_NEAR(pressed.lightness(),
              base.lightness() - Theme::pressed_lightness_step, 2.0e-3F);
  EXPECT_NEAR(pressed.hue(), base.hue(), 0.5F);
  expect_colour(theme.palette(ColourRole::on_accent).light,
                Colour{1.0F, 1.0F, 1.0F, 1.0F});
  const Oklch selection =
      Oklch::from(theme.palette(ColourRole::selected_content_background).light);
  EXPECT_NEAR(selection.hue(), base.hue(), 1.0F);
  EXPECT_LT(selection.lightness(), base.lightness());
  EXPECT_EQ(theme.colour(ColourRole::text_primary, ColorScheme::dark),
            theme.palette(ColourRole::label).dark);
  EXPECT_EQ(theme.colour(Colour::hex(0x123456), ColorScheme::dark),
            Colour::hex(0x123456));
}

TEST_F(ThemeTest, TheDefaultSeedReproducesTheMacReferences) {
  const Theme theme = Theme::defaults();
  expect_colour(theme.palette(ColourRole::accent).light, Colour::hex(0x007AFF));
  expect_colour(theme.palette(ColourRole::selected_content_background).light,
                Colour::hex(0x0064E1), 2.0F * channel);
  expect_colour(theme.palette(ColourRole::selected_content_background).dark,
                Colour::hex(0x0059D1), 2.0F * channel);
  expect_colour(theme.palette(ColourRole::selected_text_background).light,
                Colour::hex(0xB3D7FF), 2.0F * channel);
  expect_colour(theme.palette(ColourRole::link).light, Colour::hex(0x0068DA));
  expect_colour(theme.palette(ColourRole::red).light, Colour::hex(0xFF383C));
  expect_colour(theme.palette(ColourRole::red).dark, Colour::hex(0xFF4245));
  expect_colour(theme.variants(ColourRole::red).high_contrast_light,
                Colour::hex(0xE9152D));
  expect_colour(theme.variants(ColourRole::system_background).dark,
                Colour::hex(0x000000));
  expect_colour(theme.variants(ColourRole::system_background).dark_elevated,
                Colour::hex(0x1C1C1E));
  expect_colour(theme.palette(ColourRole::system_background).dark,
                Colour::hex(0x1C1C1E));
}

TEST_F(ThemeTest, OldRoleNamesMapOntoTheSemanticPalette) {
  EXPECT_EQ(ColourRole::background, ColourRole::system_grouped_background);
  EXPECT_EQ(ColourRole::surface, ColourRole::secondary_system_background);
  EXPECT_EQ(ColourRole::surface_raised,
            ColourRole::secondary_system_grouped_background);
  EXPECT_EQ(ColourRole::text_primary, ColourRole::label);
  EXPECT_EQ(ColourRole::text_secondary, ColourRole::secondary_label);
  EXPECT_EQ(ColourRole::focus_ring, ColourRole::keyboard_focus_indicator);
  EXPECT_EQ(ColourRole::destructive, ColourRole::red);
  EXPECT_EQ(Color::secondary, ColourValue{ColourRole::secondary_label});
  EXPECT_EQ(Color::tint, ColourValue{ColourRole::accent});
}

TEST_F(ThemeTest, EveryRoleResolvesOpaqueInBothSchemes) {
  const Theme theme = Theme::defaults();
  for (std::size_t index = 0; index < colour_role_count; ++index) {
    const auto role = static_cast<ColourRole>(index);
    if (role == ColourRole::scrim) {
      continue;
    }
    EXPECT_FLOAT_EQ(theme.palette(role).light.alpha, 1.0F) << index;
    EXPECT_FLOAT_EQ(theme.palette(role).dark.alpha, 1.0F) << index;
    const AppearanceVariants& every = theme.variants(role);
    for (const Colour& column :
         {every.light, every.dark, every.dark_elevated,
          every.high_contrast_light, every.high_contrast_dark,
          every.high_contrast_dark_elevated}) {
      EXPECT_FLOAT_EQ(column.alpha, 1.0F) << index;
    }
  }
}

TEST_F(ThemeTest, AppleStepsClearTheContrastCheck) {
  const Theme theme = Theme::defaults();
  const auto apart = [&theme](ColourRole first, ColourRole second,
                              ColorScheme scheme) {
    const Oklab one = Oklab::from(theme.colour(first, scheme));
    const Oklab two = Oklab::from(theme.colour(second, scheme));
    return std::hypot(one.lightness() - two.lightness(), one.a() - two.a(),
                      one.b() - two.b());
  };
  const auto lightness = [&theme](ColourRole role, ColorScheme scheme) {
    return Oklch::from(theme.colour(role, scheme)).lightness();
  };
  constexpr std::array ladder{
      ColourRole::system_fill, ColourRole::secondary_system_fill,
      ColourRole::tertiary_system_fill, ColourRole::quaternary_system_fill,
      ColourRole::quinary_system_fill};
  constexpr float fill = 0.035F;
  constexpr float border = 0.035F;
  for (const ColorScheme scheme : {ColorScheme::light, ColorScheme::dark}) {
    const bool dark = scheme == ColorScheme::dark;
    float previous = dark ? 2.0F : -1.0F;
    for (const ColourRole role : ladder) {
      const float current = lightness(role, scheme);
      EXPECT_TRUE(dark ? current < previous : current > previous);
      previous = current;
    }
    for (const ColourRole role :
         {ColourRole::system_fill, ColourRole::secondary_system_fill,
          ColourRole::tertiary_system_fill}) {
      EXPECT_GE(apart(role, ColourRole::control_background, scheme), fill);
    }
    EXPECT_GE(
        apart(ColourRole::separator, ColourRole::control_background, scheme),
        border);
    EXPECT_GE(apart(ColourRole::surface_raised, ColourRole::background, scheme),
              fill);
    EXPECT_GE(apart(ColourRole::unemphasized_selected_content_background,
                    ColourRole::background, scheme),
              fill);
  }
}

TEST_F(ThemeTest, LabelsAndSystemColoursStayReadable) {
  const Theme theme = Theme::defaults();
  const auto luminance = [](Colour colour) {
    const auto linear = [](float value) {
      return value <= 0.04045F ? value / 12.92F
                               : std::pow((value + 0.055F) / 1.055F, 2.4F);
    };
    return (0.2126F * linear(colour.red)) + (0.7152F * linear(colour.green)) +
           (0.0722F * linear(colour.blue));
  };
  const auto contrast = [&](Colour first, Colour second) {
    const float top = luminance(first) + 0.05F;
    const float bottom = luminance(second) + 0.05F;
    return std::max(top, bottom) / std::min(top, bottom);
  };
  constexpr std::array hues{
      ColourRole::red,    ColourRole::orange, ColourRole::yellow,
      ColourRole::green,  ColourRole::mint,   ColourRole::teal,
      ColourRole::cyan,   ColourRole::blue,   ColourRole::indigo,
      ColourRole::purple, ColourRole::pink,   ColourRole::brown};
  for (const ColorScheme scheme : {ColorScheme::light, ColorScheme::dark}) {
    for (const ColourRole ground :
         {ColourRole::window_background, ColourRole::control_background,
          ColourRole::system_background}) {
      const Colour behind = theme.colour(ground, scheme);
      EXPECT_GE(contrast(theme.colour(ColourRole::label, scheme), behind),
                7.0F);
      EXPECT_GE(
          contrast(theme.colour(ColourRole::secondary_label, scheme), behind),
          4.5F);
    }
    const Colour card = theme.colour(ColourRole::control_background, scheme);
    EXPECT_GE(contrast(theme.colour(ColourRole::tertiary_label, scheme), card),
              1.8F);
    EXPECT_GE(contrast(theme.colour(ColourRole::link, scheme), card), 4.5F);
    const Colour page = theme.colour(ColourRole::system_background, scheme);
    for (std::size_t index = 0; index < hues.size(); ++index) {
      const Colour hue = theme.colour(hues[index], scheme);
      EXPECT_GE(contrast(hue, page), 1.4F) << index;
      const Colour next = theme.colour(hues[(index + 1) % hues.size()], scheme);
      const Oklab here = Oklab::from(hue);
      const Oklab there = Oklab::from(next);
      EXPECT_GE(std::hypot(here.lightness() - there.lightness(),
                           here.a() - there.a(), here.b() - there.b()),
                0.02F)
          << index;
    }
    EXPECT_GE(contrast(theme.colour(ColourRole::on_accent, scheme),
                       theme.colour(ColourRole::accent, scheme)),
              3.0F);
  }
  EXPECT_GT(theme.elevation(Elevation::low).key.opacity, 0.25F);
}

TEST_F(ThemeTest, LightAccentsGetDarkInk) {
  const Theme theme = Theme::from(Seed{.accent = Colour::hex(0xFFD60A)});
  const Oklch ink = Oklch::from(theme.palette(ColourRole::on_accent).light);
  EXPECT_LT(ink.lightness(), 0.3F);
}

TEST_F(ThemeTest, TokensResolveThroughTheTheme) {
  const Theme theme = Theme::defaults();
  EXPECT_FLOAT_EQ(theme.space(space::md), 12.0F);
  EXPECT_FLOAT_EQ(theme.space(5.0F), 5.0F);
  EXPECT_FLOAT_EQ(theme.radius(radius::card), 14.0F);
  EXPECT_TRUE(std::isinf(theme.radius(radius::capsule)));
  EXPECT_FLOAT_EQ(theme.type(TextRole::large_title).size, 26.0F);
  EXPECT_EQ(theme.type(TextRole::headline).weight, 700);
  EXPECT_FLOAT_EQ(theme.type(TextRole::caption).line_height, 13.0F);
  EXPECT_GT(theme.elevation(Elevation::high).key.blur,
            theme.elevation(Elevation::low).key.blur);
  const Edges edges = theme.edges(Insets::symmetric(space::lg, space::xs));
  EXPECT_FLOAT_EQ(edges.horizontal(), 32.0F);
  EXPECT_FLOAT_EQ(edges.vertical(), 8.0F);
  const Theme edited = theme.with(Theme::Radii{
      .control = 4.0F, .card = 10.0F, .sheet = 16.0F, .continuous = false});
  EXPECT_FLOAT_EQ(edited.radius(radius::card), 10.0F);
  EXPECT_FLOAT_EQ(theme.radius(radius::card), 14.0F);
  EXPECT_NE(edited, theme);
}

TEST_F(ThemeTest, SchemeAndThemeChangesMarkReadersDirty) {
  auto dark = std::make_shared<composition::Observable<bool>>(false);
  auto warm = std::make_shared<composition::Observable<bool>>(false);
  auto reads = std::make_shared<int>(0);
  composition::Tree tree{Switcher{dark, warm, reads}, [] { return true; }};
  tree.reconcile();
  EXPECT_EQ(*reads, 1);
  EXPECT_EQ(label(tree), "light");
  dark->set(true);
  tree.reconcile();
  EXPECT_EQ(*reads, 2);
  EXPECT_EQ(label(tree), "dark");
  warm->set(true);
  tree.reconcile();
  EXPECT_EQ(*reads, 3);
  EXPECT_EQ(label(tree), "dark warm");
  const std::uint64_t revision = tree.revision();
  tree.reconcile();
  EXPECT_EQ(*reads, 3);
  EXPECT_EQ(tree.revision(), revision);
}

}
