#include <arisa/provider/gpu/external_texture.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/headless/software_raster.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace arisa::provider::headless {

class ExternalTextureTest : public ::testing::Test {
 protected:
  using Canvas = SoftwareRaster::Canvas;

  static constexpr std::uint32_t target_width = 8;
  static constexpr std::uint32_t target_height = 2;

  static auto image(std::uint32_t width, std::uint32_t height)
      -> std::shared_ptr<Canvas> {
    auto made = std::make_shared<Canvas>(width, height);
    made->fill(Colour{.red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 1.0F});
    return made;
  }

  static auto described(std::uint32_t width, std::uint32_t height,
                        SamplerFilter filter = SamplerFilter::linear)
      -> ImportDescription {
    return ImportDescription{.width = width,
                             .height = height,
                             .format = TextureFormat::rgba8,
                             .filter = filter,
                             .premultiplied = false};
  }

  static auto stretched(SamplerFilter filter) -> Canvas {
    Canvas source{2, 1};
    const std::array<std::byte, 8> pixels{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0xff},
        std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}};
    source.upload(TextureFormat::rgba8,
                  TextureRegion{.x = 0, .y = 0, .width = 2, .height = 1},
                  PixelRows{pixels});
    Canvas target{target_width, target_height};
    target.fill(
        Colour{.red = 1.0F, .green = 0.0F, .blue = 0.0F, .alpha = 1.0F});
    const auto right = static_cast<float>(target_width);
    const auto bottom = static_cast<float>(target_height);
    const ShapeVertex base{.colour = 0xFFFFFFFFU, .kind = ShapeKind::image};
    std::array<ShapeVertex, 4> vertices{base, base, base, base};
    vertices[0].position = {0.0F, 0.0F};
    vertices[0].uv = {0.0F, 0.0F};
    vertices[1].position = {right, 0.0F};
    vertices[1].uv = {1.0F, 0.0F};
    vertices[2].position = {right, bottom};
    vertices[2].uv = {1.0F, 1.0F};
    vertices[3].position = {0.0F, bottom};
    vertices[3].uv = {0.0F, 1.0F};
    const std::array<Index, 6> indices{0, 1, 2, 2, 3, 0};
    const std::array<DrawCommand, 1> commands{DrawCommand{
        .texture = TextureId{1},
        .clip =
            {.x = 0, .y = 0, .width = target_width, .height = target_height},
        .index_offset = 0,
        .index_count = 6,
        .vertex_offset = 0}};
    SoftwareRaster::draw(
        target,
        PassRegion{.x = 0.0F, .y = 0.0F, .width = right, .height = bottom},
        vertices, indices, commands, [&](TextureId) {
          return SoftwareRaster::Source{.canvas = &source, .filter = filter};
        });
    return target;
  }

  static auto upload(TextureFormat format, std::span<const std::byte> pixel)
      -> SoftwareRaster::Rgba {
    Canvas canvas{1, 1};
    canvas.upload(format,
                  TextureRegion{.x = 0, .y = 0, .width = 1, .height = 1},
                  PixelRows{pixel});
    return canvas.texel(0, 0);
  }

  static void present(NullGpu& gpu, TextureId texture) {
    auto frame = gpu.begin_frame();
    ASSERT_TRUE(frame.has_value());
    auto geometry = frame->allocate_shapes(4, 6);
    ASSERT_TRUE(geometry.has_value());
    const std::array<Index, 6> indices{0, 1, 2, 0, 2, 3};
    std::ranges::copy(indices, geometry->indices.begin());
    const std::array<DrawCommand, 1> commands{
        DrawCommand{.texture = texture,
                    .clip = {.x = 0, .y = 0, .width = 8, .height = 8},
                    .index_offset = 0,
                    .index_count = 6,
                    .vertex_offset = 0}};
    ASSERT_TRUE(
        frame->draw(ShapeDrawData{.geometry = *geometry, .commands = commands})
            .has_value());
    ASSERT_TRUE(frame->present().has_value());
  }
};

TEST_F(ExternalTextureTest, ImportIsCountedAndNeverCopies) {
  NullGpu gpu;
  auto imported = gpu.import_texture(image(4, 2), described(4, 2));
  ASSERT_TRUE(imported.has_value());
  EXPECT_EQ(gpu.imports(), 1U);
  EXPECT_EQ(gpu.uploads(), 0U);
  EXPECT_EQ(gpu.uploaded_bytes(), 0U);
  EXPECT_EQ(gpu.live_textures(), 1U);
  ASSERT_TRUE(gpu.publications().has_value());
  const auto first = gpu.publications()->get().find(imported->id());
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->generation, 0U);
  ASSERT_TRUE(gpu.frame_schedule().has_value());
  static_cast<void>(gpu.frame_schedule()->get().take_request());
  imported->publish();
  EXPECT_TRUE(gpu.frame_schedule()->get().take_request());
  EXPECT_EQ(gpu.publications()->get().find(imported->id())->generation, 1U);
  EXPECT_EQ(gpu.uploads(), 0U);
}

TEST_F(ExternalTextureTest, RejectsAnImageThatDoesNotMatchItsDescription) {
  NullGpu gpu;
  EXPECT_FALSE(gpu.import_texture(image(4, 2), described(2, 2)).has_value());
  EXPECT_FALSE(gpu.import_texture(nullptr, described(2, 2)).has_value());
  EXPECT_FALSE(gpu.import_texture(image(4, 2), described(0, 0)).has_value());
  EXPECT_EQ(gpu.live_textures(), 0U);
}

TEST_F(ExternalTextureTest, InUseHoldsUntilTheSamplingFrameCompletes) {
  NullGpu gpu;
  auto imported = gpu.import_texture(image(4, 4), described(4, 4));
  ASSERT_TRUE(imported.has_value());
  EXPECT_FALSE(imported->in_use());
  gpu.hold_completion(2);
  present(gpu, imported->id());
  EXPECT_TRUE(imported->in_use());
  present(gpu, imported->id());
  present(gpu, imported->id());
  EXPECT_TRUE(imported->in_use());
  gpu.hold_completion(0);
  EXPECT_FALSE(imported->in_use());
}

TEST_F(ExternalTextureTest, DestroyingTheHandleMidFlightDefersTheRelease) {
  NullGpu gpu;
  bool released = false;
  std::optional<NullGpu::Imported> imported;
  {
    auto made = gpu.import_texture(
        image(4, 4), described(4, 4),
        support::ScopedCallback{[&released]() noexcept { released = true; }});
    ASSERT_TRUE(made.has_value());
    imported.emplace(std::move(*made));
  }
  gpu.hold_completion(3);
  present(gpu, imported->id());
  imported.reset();
  EXPECT_FALSE(released);
  EXPECT_EQ(gpu.live_textures(), 1U);
  ASSERT_TRUE(gpu.publications().has_value());
  EXPECT_EQ(gpu.publications()->get().live(), 1U);
  gpu.hold_completion(0);
  EXPECT_TRUE(released);
  EXPECT_EQ(gpu.live_textures(), 0U);
  EXPECT_EQ(gpu.publications()->get().live(), 0U);
}

TEST_F(ExternalTextureTest, DestroyingAnIdleHandleReleasesAtOnce) {
  NullGpu gpu;
  bool released = false;
  {
    auto made = gpu.import_texture(
        image(2, 2), described(2, 2),
        support::ScopedCallback{[&released]() noexcept { released = true; }});
    ASSERT_TRUE(made.has_value());
    present(gpu, made->id());
  }
  EXPECT_TRUE(released);
  EXPECT_EQ(gpu.live_textures(), 0U);
}

TEST_F(ExternalTextureTest, NearestSamplingKeepsTexelsWhereLinearBlends) {
  const Canvas nearest = stretched(SamplerFilter::nearest);
  const Canvas linear = stretched(SamplerFilter::linear);
  EXPECT_FLOAT_EQ(nearest.texel(3, 0)[0], 0.0F);
  EXPECT_FLOAT_EQ(nearest.texel(4, 0)[0], 1.0F);
  EXPECT_FLOAT_EQ(nearest.texel(0, 0)[0], 0.0F);
  EXPECT_FLOAT_EQ(nearest.texel(7, 1)[0], 1.0F);
  EXPECT_GT(linear.texel(3, 0)[0], 0.05F);
  EXPECT_LT(linear.texel(3, 0)[0], 0.95F);
  EXPECT_GT(linear.texel(4, 0)[0], 0.05F);
  EXPECT_LT(linear.texel(4, 0)[0], 0.95F);
}

TEST_F(ExternalTextureTest, Bgra8AndRgb565ConvertToTheSameColours) {
  const std::array<std::byte, 4> bgra{std::byte{0x20}, std::byte{0x80},
                                      std::byte{0xF0}, std::byte{0xFF}};
  const SoftwareRaster::Rgba swapped = upload(TextureFormat::bgra8, bgra);
  EXPECT_NEAR(swapped[0], 0xF0 / 255.0F, 1.0e-3F);
  EXPECT_NEAR(swapped[1], 0x80 / 255.0F, 1.0e-3F);
  EXPECT_NEAR(swapped[2], 0x20 / 255.0F, 1.0e-3F);
  EXPECT_NEAR(swapped[3], 1.0F, 1.0e-3F);
  const std::array<std::array<std::byte, 2>, 3> packed{
      {{std::byte{0x00}, std::byte{0xF8}},
       {std::byte{0xE0}, std::byte{0x07}},
       {std::byte{0x1F}, std::byte{0x00}}}};
  for (std::size_t channel = 0; channel < packed.size(); ++channel) {
    const SoftwareRaster::Rgba seen =
        upload(TextureFormat::rgb565, packed[channel]);
    for (std::size_t other = 0; other < 3; ++other) {
      EXPECT_FLOAT_EQ(seen[other], other == channel ? 1.0F : 0.0F)
          << channel << " " << other;
    }
    EXPECT_FLOAT_EQ(seen[3], 1.0F);
  }
}

TEST_F(ExternalTextureTest, NullGpuTexturesAcceptTheNewFormats) {
  NullGpu gpu;
  gpu.rasterize(
      SurfaceSize{.width = SurfaceWidth{4}, .height = SurfaceHeight{4}});
  auto rgb565 = gpu.create_texture(
      {.width = 2, .height = 1, .format = TextureFormat::rgb565});
  ASSERT_TRUE(rgb565.has_value());
  const std::array<std::byte, 4> two{std::byte{0x00}, std::byte{0xF8},
                                     std::byte{0x1F}, std::byte{0x00}};
  EXPECT_TRUE(gpu.update_texture(*rgb565,
                                 {.x = 0, .y = 0, .width = 2, .height = 1}, two)
                  .has_value());
  EXPECT_EQ(gpu.uploaded_bytes(), 4U);
  auto bgra = gpu.create_texture({.width = 1,
                                  .height = 1,
                                  .format = TextureFormat::bgra8,
                                  .usage = TextureUsage::sampled,
                                  .filter = SamplerFilter::nearest});
  ASSERT_TRUE(bgra.has_value());
  EXPECT_EQ(bgra->description().filter, SamplerFilter::nearest);
}

}
