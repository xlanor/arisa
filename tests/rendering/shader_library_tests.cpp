#include <arisa/graphics/shader.hpp>
#include <arisa/theme/colour.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace arisa::graphics {

TEST(ShaderLibraryTest, LibrariesFindFunctionsByName) {
  const ShaderLibrary library = ShaderLibrary::default_library();
  EXPECT_EQ(library.name(), "default");
  const ShaderLibrary gallery = ShaderLibrary::bundle("arisa_gallery_shaders");
  EXPECT_EQ(gallery.name(), "arisa_gallery_shaders");
  ASSERT_FALSE(gallery.functions().empty());
  const std::optional<ShaderFunction> bands = gallery["bands"];
  ASSERT_TRUE(bands.has_value());
  EXPECT_EQ(bands->name(), "bands");
  EXPECT_EQ(bands->library(), "arisa_gallery_shaders");
  EXPECT_EQ(bands->kind(), ShaderFunction::Kind::colour);
  const ShaderFunction looked{gallery, "bands"};
  EXPECT_TRUE(looked.found());
  EXPECT_EQ(looked, *bands);
  EXPECT_FALSE((ShaderFunction{gallery, "absent"}).found());
  EXPECT_FALSE(gallery["absent"].has_value());
  EXPECT_EQ(gallery, ShaderLibrary::bundle("arisa_gallery_shaders"));
  EXPECT_FALSE(gallery == ShaderLibrary::bundle("missing"));
}

TEST(ShaderLibraryTest, ArgumentsPackArraysDataAndBounds) {
  const std::array<float, 3> weights{0.1F, 0.2F, 0.3F};
  const std::array<theme::Colour, 1> palette{theme::Colour::hex(0xFF0000)};
  const std::array<std::byte, 4> bytes{std::byte{1}, std::byte{2}, std::byte{3},
                                       std::byte{4}};
  const Shader shader = Shader{ShaderFunction{0, ShaderFunction::Kind::colour}}
                            .argument(Shader::Argument::float_array(weights))
                            .argument(Shader::Argument::color_array(palette))
                            .argument(Shader::Argument::data(bytes))
                            .argument(Shader::Argument::bounding_rect())
                            .argument(Shader::Argument::float2(1.0F, 2.0F))
                            .dithers_color(true);
  const std::vector<float>& packed = shader.arguments();
  ASSERT_GE(packed.size(), 4U);
  EXPECT_EQ(packed[0], 3.0F);
  EXPECT_EQ(packed[1], 0.1F);
  EXPECT_EQ(packed[4], 1.0F);
  EXPECT_EQ(packed[5], 1.0F);
  ASSERT_EQ(shader.bounds_slots().size(), 1U);
  EXPECT_TRUE(shader.dithers_color());
  EXPECT_TRUE(Shader::Argument::bounding_rect().bounds());
  EXPECT_EQ(Shader::Argument::float_value(2.0F).values(), std::vector{2.0F});
}

TEST(ShaderLibraryTest, CompileChecksUsageAgainstKind) {
  const Shader colour{ShaderFunction{0, ShaderFunction::Kind::colour}};
  EXPECT_TRUE(colour.compile(Shader::UsageType::color_effect).has_value());
  EXPECT_TRUE(colour.compile(Shader::UsageType::shape_style).has_value());
  const auto refused = colour.compile(Shader::UsageType::distortion_effect);
  ASSERT_FALSE(refused.has_value());
  EXPECT_EQ(refused.error().usage, Shader::UsageType::distortion_effect);
  EXPECT_FALSE(refused.error().missing);
  const Shader missing{
      ShaderFunction{ShaderFunction::missing, ShaderFunction::Kind::colour}};
  const auto absent = missing.compile(Shader::UsageType::color_effect);
  ASSERT_FALSE(absent.has_value());
  EXPECT_TRUE(absent.error().missing);
}

}
