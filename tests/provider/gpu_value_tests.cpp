#include <arisa/provider/gpu/gpu_frame.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/gpu/texture_registry.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace arisa::provider {

TEST(TextureTest, UploadSizeFollowsTheRegionAndFormat) {
  const TextureDescription rgba{.width = 8, .height = 4};
  const TextureDescription alpha{
      .width = 8, .height = 4, .format = TextureFormat::alpha8};
  EXPECT_EQ(
      Texture::upload_size(rgba, {.x = 0, .y = 0, .width = 8, .height = 4}),
      std::optional<std::size_t>{128});
  EXPECT_EQ(
      Texture::upload_size(alpha, {.x = 2, .y = 1, .width = 3, .height = 2}),
      std::optional<std::size_t>{6});
  EXPECT_FALSE(
      Texture::upload_size(rgba, {.x = 6, .y = 0, .width = 3, .height = 1})
          .has_value());
  EXPECT_FALSE(
      Texture::upload_size(rgba, {.x = 0, .y = 0, .width = 0, .height = 1})
          .has_value());
  EXPECT_FALSE(
      Texture::upload_size(rgba, {.x = 8, .y = 0, .width = 1, .height = 1})
          .has_value());
}

TEST(TextureTest, DescribableNeedsAPositiveBoundedExtent) {
  EXPECT_TRUE(Texture::describable({.width = 1, .height = 1}));
  EXPECT_FALSE(Texture::describable({.width = 0, .height = 1}));
  EXPECT_FALSE(Texture::describable(
      {.width = Texture::maximum_extent + 1, .height = 1}));
}

TEST(TextureRegistryTest, ReleasesOnDestructionAndRetiresSafely) {
  int destroyed = 0;
  auto registry = std::make_shared<TextureRegistry<int>>(
      [&destroyed](int&) noexcept { ++destroyed; });
  auto first =
      TextureRegistry<int>::adopt(registry, {.width = 1, .height = 1}, 7);
  auto second =
      TextureRegistry<int>::adopt(registry, {.width = 2, .height = 2}, 9);
  EXPECT_NE(first.id(), second.id());
  EXPECT_EQ(registry->find(second.id())->native, 9);
  EXPECT_EQ(registry->size(), 2U);
  {
    const Texture moved{std::move(first)};
    EXPECT_FALSE(first.valid());
    EXPECT_TRUE(moved.valid());
  }
  EXPECT_EQ(destroyed, 1);
  registry->retire();
  EXPECT_EQ(destroyed, 2);
  EXPECT_TRUE(registry->retired());
  registry.reset();
  second = Texture{TextureId{0}, {}, support::ScopedCallback{}};
  EXPECT_EQ(destroyed, 2);
}

TEST(ImageTest, ValidatesItsBufferAndReadsPixels) {
  EXPECT_FALSE(Image::create(2, 2, std::vector<std::byte>(15)).has_value());
  EXPECT_FALSE(Image::create(0, 2, {}).has_value());
  std::vector<std::byte> pixels(2 * 1 * 4);
  pixels[4] = std::byte{10};
  pixels[7] = std::byte{255};
  const auto image = Image::create(2, 1, std::move(pixels));
  ASSERT_TRUE(image.has_value());
  EXPECT_EQ(image->pixel(1, 0), (std::array<std::uint8_t, 4>{10, 0, 0, 255}));
  EXPECT_FALSE(image->pixel(2, 0).has_value());
}

TEST(GpuFrameTest, WellFormedChecksRangesAgainstTheGeometry) {
  std::array<Vertex, 4> vertices{};
  std::array<Index, 6> indices{0, 1, 2, 0, 2, 3};
  const FrameGeometry geometry{.vertices = vertices, .indices = indices};
  const std::array good{DrawCommand{.index_offset = 0, .index_count = 6}};
  EXPECT_TRUE(GpuFrame::well_formed({.geometry = geometry, .commands = good}));
  const std::array overrun{DrawCommand{.index_offset = 3, .index_count = 6}};
  EXPECT_FALSE(
      GpuFrame::well_formed({.geometry = geometry, .commands = overrun}));
  const std::array outside{
      DrawCommand{.index_offset = 0, .index_count = 6, .vertex_offset = 4}};
  EXPECT_FALSE(
      GpuFrame::well_formed({.geometry = geometry, .commands = outside}));
}

TEST(PixelRowsTest, CoversTightAndStridedRows) {
  const std::vector<std::byte> tight(12);
  EXPECT_TRUE(PixelRows{tight}.covers(4, 3));
  EXPECT_FALSE(PixelRows{tight}.covers(4, 2));
  const std::vector<std::byte> atlas(40);
  const PixelRows strided = PixelRows::strided(std::span{atlas}.subspan(6), 10);
  EXPECT_TRUE(strided.covers(4, 3));
  EXPECT_FALSE(strided.covers(4, 5));
  EXPECT_FALSE(PixelRows::strided(atlas, 3).covers(4, 2));
  EXPECT_EQ(strided.row(2, 4).data(), atlas.data() + 26);
}

TEST(GpuFrameTest, InactiveFrameRejectsWork) {
  GpuFrame frame{nullptr};
  EXPECT_FALSE(frame.active());
  frame.clear(Colour{});
  EXPECT_FALSE(frame.allocate_geometry(4, 6).has_value());
  EXPECT_FALSE(frame.allocate_shapes(4, 6).has_value());
  EXPECT_FALSE(frame.draw(DrawData{}).has_value());
  EXPECT_FALSE(frame.draw(ShapeDrawData{}).has_value());
  EXPECT_FALSE(frame.present().has_value());
}

}
