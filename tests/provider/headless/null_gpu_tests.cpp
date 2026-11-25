#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/headless/software_raster.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

namespace arisa::provider::headless {

class NullGpuTest : public ::testing::Test {
 protected:
  static auto quad(TextureId texture) -> std::array<DrawCommand, 1> {
    return {DrawCommand{.texture = texture,
                        .clip = {.x = 0, .y = 0, .width = 8, .height = 8},
                        .index_offset = 0,
                        .index_count = 6,
                        .vertex_offset = 0}};
  }

  static auto quad_geometry(GpuFrame& frame) -> FrameGeometry {
    auto geometry = frame.allocate_geometry(4, 6);
    EXPECT_TRUE(geometry.has_value());
    if (!geometry.has_value()) {
      return {};
    }
    const std::array<Index, 6> indices{0, 1, 2, 0, 2, 3};
    std::ranges::copy(indices, geometry->indices.begin());
    return *geometry;
  }
};

TEST_F(NullGpuTest, CountsTexturesAndUploads) {
  NullGpu gpu;
  {
    auto texture = gpu.create_texture({.width = 2, .height = 2});
    ASSERT_TRUE(texture.has_value());
    EXPECT_EQ(gpu.live_textures(), 1U);
    const std::vector<std::byte> pixels(16);
    EXPECT_TRUE(gpu.update_texture(*texture,
                                   {.x = 0, .y = 0, .width = 2, .height = 2},
                                   pixels)
                    .has_value());
    EXPECT_FALSE(gpu.update_texture(*texture,
                                    {.x = 1, .y = 1, .width = 2, .height = 2},
                                    pixels)
                     .has_value());
    EXPECT_EQ(gpu.uploads(), 1U);
  }
  EXPECT_EQ(gpu.live_textures(), 0U);
}

TEST_F(NullGpuTest, RejectsTexturesFromAnotherProvider) {
  NullGpu first;
  NullGpu second;
  auto foreign = first.create_texture({.width = 1, .height = 1});
  ASSERT_TRUE(foreign.has_value());
  const std::array<std::byte, 4> pixel{};
  const auto updated = second.update_texture(
      *foreign, {.x = 0, .y = 0, .width = 1, .height = 1}, pixel);
  ASSERT_FALSE(updated.has_value());
  EXPECT_EQ(updated.error().code(), ProviderErrorCode::invalid_argument);
}

TEST_F(NullGpuTest, DestroyingAFrameWithoutPresentingDiscardsIt) {
  NullGpu gpu;
  auto texture = gpu.create_texture({.width = 1, .height = 1});
  ASSERT_TRUE(texture.has_value());
  const auto commands = quad(texture->id());
  {
    auto frame = gpu.begin_frame();
    ASSERT_TRUE(frame.has_value());
    frame->clear(Colour{.red = 1.0F});
    EXPECT_TRUE(
        frame->draw({.geometry = quad_geometry(*frame), .commands = commands})
            .has_value());
  }
  EXPECT_EQ(gpu.frames_discarded(), 1U);
  EXPECT_EQ(gpu.frames_presented(), 0U);
  auto frame = gpu.begin_frame();
  ASSERT_TRUE(frame.has_value());
  EXPECT_TRUE(frame->present().has_value());
  EXPECT_FALSE(frame->active());
  EXPECT_FALSE(frame->present().has_value());
  EXPECT_EQ(gpu.frames_presented(), 1U);
  EXPECT_EQ(gpu.frames_discarded(), 1U);
}

TEST_F(NullGpuTest, RenderPassRejectionsSayWhichCheckFailed) {
  NullGpu gpu;
  auto target = gpu.create_texture({.width = 8,
                                    .height = 8,
                                    .format = TextureFormat::rgba8,
                                    .usage = TextureUsage::render_target});
  ASSERT_TRUE(target.has_value());
  auto frame = gpu.begin_frame();
  ASSERT_TRUE(frame.has_value());
  auto shapes = frame->allocate_shapes(4, 6);
  ASSERT_TRUE(shapes.has_value());
  const ShapeDrawData data{.geometry = *shapes, .commands = {}};
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const auto detail = [&](RenderPass pass) {
    const auto drawn = frame->draw(data, pass);
    EXPECT_FALSE(drawn.has_value());
    return drawn.has_value() ? ProviderErrorDetail::none
                             : drawn.error().detail();
  };
  EXPECT_EQ(detail(RenderPass{.target = TextureId{0},
                              .region = {.width = 8.0F, .height = 8.0F}}),
            ProviderErrorDetail::zero_target);
  EXPECT_EQ(detail(RenderPass{.target = target->id(),
                              .region = {.width = 0.0F, .height = 8.0F}}),
            ProviderErrorDetail::empty_region);
  EXPECT_EQ(detail(RenderPass{.target = target->id(),
                              .region = {.width = nan, .height = 8.0F}}),
            ProviderErrorDetail::non_finite_region);
  EXPECT_EQ(detail(RenderPass{.target = TextureId{target->id().value() + 1000},
                              .region = {.width = 8.0F, .height = 8.0F}}),
            ProviderErrorDetail::unknown_target);
  GpuFrame empty{std::unique_ptr<GpuFrame::Implementation>{}};
  const auto unopened =
      empty.draw(data, RenderPass{.target = target->id(),
                                  .region = {.width = 8.0F, .height = 8.0F}});
  ASSERT_FALSE(unopened.has_value());
  EXPECT_EQ(unopened.error().detail(), ProviderErrorDetail::no_frame);
}

TEST_F(NullGpuTest, DrawRejectsUnknownTexturesAndMalformedData) {
  NullGpu gpu;
  auto frame = gpu.begin_frame();
  ASSERT_TRUE(frame.has_value());
  const FrameGeometry geometry = quad_geometry(*frame);
  const auto unknown = quad(TextureId{0});
  EXPECT_FALSE(
      frame->draw({.geometry = geometry, .commands = unknown}).has_value());
  auto texture = gpu.create_texture({.width = 1, .height = 1});
  ASSERT_TRUE(texture.has_value());
  const auto known = quad(texture->id());
  FrameGeometry truncated = geometry;
  truncated.vertices = truncated.vertices.first(2);
  EXPECT_FALSE(
      frame->draw({.geometry = truncated, .commands = known}).has_value());
  std::array<Vertex, 4> foreign_vertices{};
  std::array<Index, 6> foreign_indices{};
  EXPECT_FALSE(frame
                   ->draw({.geometry = {.vertices = foreign_vertices,
                                        .indices = foreign_indices},
                           .commands = known})
                   .has_value());
  EXPECT_TRUE(
      frame->draw({.geometry = geometry, .commands = known}).has_value());
  EXPECT_EQ(gpu.draws(), 1U);
}

TEST_F(NullGpuTest, TexturesAndFramesMayOutliveTheProvider) {
  std::optional<Texture> texture;
  std::optional<GpuFrame> frame;
  {
    NullGpu gpu;
    auto created = gpu.create_texture({.width = 4, .height = 4});
    ASSERT_TRUE(created.has_value());
    texture.emplace(std::move(*created));
    auto begun = gpu.begin_frame();
    ASSERT_TRUE(begun.has_value());
    frame.emplace(std::move(*begun));
  }
  EXPECT_TRUE(texture->valid());
  EXPECT_TRUE(frame->present().has_value());
  texture.reset();
  SUCCEED();
}

TEST_F(NullGpuTest, CaptureIsUnsupported) {
  NullGpu gpu;
  const auto image = gpu.capture();
  ASSERT_FALSE(image.has_value());
  EXPECT_EQ(image.error().code(), ProviderErrorCode::unsupported_capability);
}

TEST_F(NullGpuTest, TexturesRetiredWhileAFrameIsOpenOutliveThatFrame) {
  NullGpu gpu;
  const auto canvas = std::make_shared<SoftwareRaster::Canvas>(2, 2);
  auto frame = gpu.begin_frame();
  ASSERT_TRUE(frame.has_value());
  {
    auto imported =
        gpu.import_texture(canvas, ImportDescription{.width = 2, .height = 2});
    ASSERT_TRUE(imported.has_value());
    EXPECT_EQ(canvas.use_count(), 2);
  }
  EXPECT_EQ(gpu.live_textures(), 0U);
  EXPECT_EQ(canvas.use_count(), 2);
  auto packet = frame->seal();
  ASSERT_TRUE(packet.has_value());
  EXPECT_FALSE(frame->active());
  EXPECT_EQ(canvas.use_count(), 2);
  ASSERT_TRUE(packet->submit().has_value());
  EXPECT_EQ(canvas.use_count(), 1);
  EXPECT_EQ(gpu.frames_presented(), 1U);
}

TEST_F(NullGpuTest, ASecondOpenFrameIsBuiltOutsideTheSlots) {
  NullGpu gpu;
  auto first = gpu.begin_frame();
  ASSERT_TRUE(first.has_value());
  auto second = gpu.begin_frame();
  ASSERT_TRUE(second.has_value());
  EXPECT_TRUE(second->present().has_value());
  EXPECT_TRUE(first->present().has_value());
  auto third = gpu.begin_frame();
  ASSERT_TRUE(third.has_value());
  EXPECT_TRUE(third->present().has_value());
  EXPECT_EQ(gpu.frames_presented(), 3U);
}

TEST_F(NullGpuTest, AnAbandonedPacketReleasesItsSlot) {
  NullGpu gpu;
  for (int index = 0; index < 4; ++index) {
    auto frame = gpu.begin_frame();
    ASSERT_TRUE(frame.has_value());
    auto packet = frame->seal();
    ASSERT_TRUE(packet.has_value());
  }
  EXPECT_EQ(gpu.frames_discarded(), 4U);
  auto frame = gpu.begin_frame();
  ASSERT_TRUE(frame.has_value());
  EXPECT_TRUE(frame->present().has_value());
}

}
