#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/color_scheme_contrast.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_mix.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/material.hpp>
#include <arisa/theme/resolved_hdr.hpp>
#include <arisa/theme/rgb_color_space.hpp>
#include <arisa/theme/theme.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <optional>

namespace arisa::theme {

class ColourMixTest : public ::testing::Test {
 protected:
  static constexpr float level = 1.0F / 255.0F;

  static void expect_rgb(Colour actual, std::uint32_t expected,
                         float slack = 3.0F * level) {
    const Colour wanted = Colour::hex(expected);
    EXPECT_NEAR(actual.red, wanted.red, slack);
    EXPECT_NEAR(actual.green, wanted.green, slack);
    EXPECT_NEAR(actual.blue, wanted.blue, slack);
  }
};

TEST_F(ColourMixTest, MixesMatchTheSwiftUIReference) {
  const Colour red = Colour::hex(0xFF0000);
  const Colour blue = Colour::hex(0x0000FF);
  expect_rgb(ColourMix::mix(red, blue, 0.5F, ColourInterpolation::device),
             0x800080);
  expect_rgb(ColourMix::mix(red, blue, 0.5F, ColourInterpolation::perceptual),
             0x8C53A2, 6.0F * level);
  expect_rgb(ColourMix::mix(red, blue, 0.0F, ColourInterpolation::oklch),
             0xFF0000);
  expect_rgb(ColourMix::mix(red, blue, 1.0F, ColourInterpolation::linear),
             0x0000FF);
}

TEST_F(ColourMixTest, ColourSpacesConvertToSRGB) {
  expect_rgb(ColourMix::rgb(RGBColorSpace::display_p3, 0.5F, 0.5F, 0.2F),
             0x808021);
  expect_rgb(ColourMix::rgb(RGBColorSpace::srgb_linear, 0.5F, 0.2F, 0.0F),
             0xBC7C00);
  expect_rgb(ColourMix::grey(RGBColorSpace::srgb_linear, 0.5F), 0xBCBCBC);
  expect_rgb(ColourMix::rgb(RGBColorSpace::srgb, 0.25F, 0.5F, 1.0F), 0x4080FF);
  EXPECT_NEAR(ColourMix::encoded(ColourMix::decoded(0.3F)), 0.3F, 1.0e-5F);
}

TEST_F(ColourMixTest, ExposureHalvesLinearLightPerStop) {
  expect_rgb(ColourMix::exposed(Colour::hex(0x808080), -1.0F), 0x5C5C5C);
  const Colour lifted =
      ColourMix::exposed(Colour{0.25F, 0.5F, 0.1F, 1.0F}, 1.0F);
  EXPECT_GT(lifted.green, 0.5F);
  EXPECT_LE(lifted.green, 1.0F);
}

TEST_F(ColourMixTest, ColourValuesCarryMixesExposureAndHeadroom) {
  const Theme theme = Theme::defaults();
  const ColourValue mixed = ColourValue{Colour::hex(0xFF0000)}.mix(
      Colour::hex(0x0000FF), 0.5F, ColourInterpolation::device);
  EXPECT_TRUE(mixed.steps().empty());
  expect_rgb(theme.colour(mixed, ColorScheme::light), 0x800080);
  const ColourValue role = ColourValue{ColourRole::accent}.mix(
      Colour::hex(0x000000), 0.5F, ColourInterpolation::device);
  ASSERT_EQ(role.steps().size(), 1U);
  const Colour accent = theme.colour(ColourRole::accent, ColorScheme::light);
  const Colour darker = theme.colour(role, ColorScheme::light);
  EXPECT_NEAR(darker.red, accent.red * 0.5F, 2.0F * level);
  const std::optional<ColourValue> decoded =
      ColourValue::decoded(role.encoded());
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(*decoded, role);
  const ColourValue bright =
      ColourValue{Colour::hex(0x808080)}.exposure_adjust(-1.0F).headroom(2.0F);
  EXPECT_EQ(bright.headroom(), 2.0F);
  const ResolvedHDR hdr = theme.colour_hdr(bright, ColorScheme::light);
  EXPECT_EQ(hdr.headroom, 2.0F);
  EXPECT_NEAR(hdr.red, 0x5C / 255.0F, 3.0F * level);
  const ResolvedHDR plain =
      ResolvedHDR::from(Colour{0.5F, 0.5F, 0.5F, 1.0F}, 3.0F);
  EXPECT_NEAR(plain.linear_red(), 0.214F, 2.0e-3F);
  EXPECT_EQ(plain.resolved(), (Colour{0.5F, 0.5F, 0.5F, 1.0F}));
  EXPECT_EQ(plain, plain);
}

TEST_F(ColourMixTest, ColourGradientLiftsTheTopAndReachesTheColour) {
  const graphics::Gradient gradient =
      graphics::Gradient::standard(Colour::hex(0xFF0000));
  ASSERT_EQ(gradient.stops().size(), 2U);
  EXPECT_EQ(gradient.stops()[1].location, graphics::Gradient::standard_reach);
  const Theme theme = Theme::defaults();
  expect_rgb(theme.colour(gradient.stops()[0].color, ColorScheme::light),
             0xFF3333, 12.0F * level);
  expect_rgb(theme.colour(gradient.stops()[1].color, ColorScheme::light),
             0xFF0000);
}

TEST_F(ColourMixTest, StylesResolveBlendAndBound) {
  const Theme theme = Theme::defaults();
  const graphics::ShapeStyle black = graphics::ShapeStyle::black();
  ASSERT_TRUE(black.colour().has_value());
  expect_rgb(theme.colour(*black.colour(), ColorScheme::dark), 0x000000);
  ASSERT_TRUE(graphics::ShapeStyle::white().colour().has_value());
  EXPECT_EQ(graphics::ShapeStyle::bar().material(), Material::bar);
  EXPECT_NE(graphics::ShapeStyle::image(nullptr).image_paint(), nullptr);
  const graphics::ShapeStyle blended =
      graphics::ShapeStyle::blended(graphics::BlendMode::multiply);
  EXPECT_EQ(blended.blend_mode(), graphics::BlendMode::multiply);
  EXPECT_EQ(black.blend_mode(graphics::BlendMode::screen).blend_mode(),
            graphics::BlendMode::screen);
  const composition::Rect area{
      .x = 1.0F, .y = 2.0F, .width = 3.0F, .height = 4.0F};
  EXPECT_EQ(black.in(area).bounds(), std::optional{area});
  const graphics::ShapeStyle role{ColourValue{ColourRole::accent}};
  const graphics::ShapeStyle resolved = role.resolve(theme, ColorScheme::light);
  ASSERT_TRUE(resolved.colour().has_value());
  EXPECT_EQ(resolved.colour()->role(), nullptr);
}

TEST_F(ColourMixTest, BarMaterialAndSchemeCasesAreListed) {
  EXPECT_EQ(Material::names[Material::count - 1], "bar");
  EXPECT_EQ(all_color_schemes.size(), 2U);
  EXPECT_EQ(all_color_scheme_contrasts.size(), 2U);
  EXPECT_EQ(all_color_schemes[1], ColorScheme::dark);
  EXPECT_EQ(all_color_scheme_contrasts[1], ColorSchemeContrast::increased);
}

}
