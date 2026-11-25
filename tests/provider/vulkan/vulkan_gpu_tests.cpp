#include <arisa/conformance/gpu_suite.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/gpu/frame_packet.hpp>
#include <arisa/provider/gpu/gpu_timings.hpp>
#include <arisa/provider/gpu/render_pass.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/gpu/vertex.hpp>
#include <arisa/provider/standard/std_threading.hpp>
#include <arisa/provider/threading/thread.hpp>
#include <arisa/provider/threading/thread_role.hpp>
#include <arisa/provider/threading/thread_spec.hpp>
#include <arisa/provider/vulkan/memory_allocator.hpp>
#include <arisa/provider/vulkan/vulkan_buffer.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>
#include <arisa/provider/vulkan/vulkan_gpu.hpp>
#include <arisa/provider/vulkan/vulkan_image.hpp>

#include "vulkan_test.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <print>
#include <span>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

namespace arisa::provider::vulkan {

class VulkanGpuTest : public VulkanTest {
 protected:
  static constexpr SurfaceSize small{.width = SurfaceWidth{64},
                                     .height = SurfaceHeight{48}};
  static constexpr std::uint32_t white_colour = 0xffffffffU;

  static auto solid(VulkanGpu& gpu, std::array<std::uint8_t, 4> rgba)
      -> std::expected<Texture, ProviderError> {
    auto texture = gpu.create_texture({.width = 1, .height = 1});
    if (!texture.has_value()) {
      return texture;
    }
    const std::array<std::byte, 4> texel{std::byte{rgba[0]}, std::byte{rgba[1]},
                                         std::byte{rgba[2]},
                                         std::byte{rgba[3]}};
    if (auto updated = gpu.update_texture(
            *texture, {.x = 0, .y = 0, .width = 1, .height = 1}, texel);
        !updated.has_value()) {
      return std::unexpected(updated.error());
    }
    return texture;
  }

  static auto quad(float left, float top, float right, float bottom,
                   std::uint32_t colour) -> std::array<Vertex, 4> {
    return {
        Vertex{.position = {left, top}, .uv = {0.0F, 0.0F}, .colour = colour},
        Vertex{.position = {right, top}, .uv = {1.0F, 0.0F}, .colour = colour},
        Vertex{
            .position = {right, bottom}, .uv = {1.0F, 1.0F}, .colour = colour},
        Vertex{
            .position = {left, bottom}, .uv = {0.0F, 1.0F}, .colour = colour}};
  }

  static void show(VulkanGpu& gpu, TextureId texture, Colour clear = {}) {
    const std::uint32_t white = Vertex::rgba(255, 255, 255, 255);
    const auto vertices = quad(0.0F, 0.0F, 64.0F, 48.0F, white);
    const std::array<Index, 6> indices{0, 1, 2, 0, 2, 3};
    const std::array commands{
        DrawCommand{.texture = texture,
                    .clip = ClipRect{.x = 0, .y = 0, .width = 64, .height = 48},
                    .index_offset = 0,
                    .index_count = 6,
                    .vertex_offset = 0}};
    auto frame = gpu.begin_frame();
    ASSERT_TRUE(frame.has_value());
    frame->clear(clear);
    auto geometry = frame->allocate_geometry(vertices.size(), indices.size());
    ASSERT_TRUE(geometry.has_value());
    std::ranges::copy(vertices, geometry->vertices.begin());
    std::ranges::copy(indices, geometry->indices.begin());
    ASSERT_TRUE(
        frame->draw(DrawData{.geometry = *geometry, .commands = commands})
            .has_value());
    ASSERT_TRUE(frame->present().has_value());
  }

  static auto shape(float left, float top, float right, float bottom,
                    std::uint32_t colour) -> std::array<ShapeVertex, 4> {
    const ShapeVertex base{.colour = colour, .kind = ShapeKind::image};
    std::array<ShapeVertex, 4> vertices{base, base, base, base};
    vertices[0].position = {left, top};
    vertices[1].position = {right, top};
    vertices[2].position = {right, bottom};
    vertices[3].position = {left, bottom};
    vertices[1].uv = {1.0F, 0.0F};
    vertices[2].uv = {1.0F, 1.0F};
    vertices[3].uv = {0.0F, 1.0F};
    return vertices;
  }

  static auto pixel(VulkanGpu& gpu, std::uint32_t x, std::uint32_t y)
      -> std::array<std::uint8_t, 4> {
    const auto image = gpu.capture();
    EXPECT_TRUE(image.has_value());
    if (!image.has_value()) {
      return {};
    }
    return image->pixel(x, y).value_or(std::array<std::uint8_t, 4>{});
  }
};

TEST_F(VulkanGpuTest, CreatesADeviceAndDescribesIt) {
  auto gpu = offscreen({.size = small, .images = 3});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  const std::string report = gpu->device().report();
  std::println("Vulkan device: {}", report);
  EXPECT_NE(report.find("Vulkan 1."), std::string::npos);
  EXPECT_GE(gpu->device().capabilities().api_version, VK_API_VERSION_1_1);
  EXPECT_TRUE(gpu->presents_off_thread());
  EXPECT_FALSE(gpu->device().capabilities().presents);
}

TEST_F(VulkanGpuTest, ClearsAndCapturesOffscreen) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  const auto before = gpu->capture();
  ASSERT_FALSE(before.has_value());
  EXPECT_EQ(before.error().code(), ProviderErrorCode::not_found);
  auto frame = gpu->begin_frame();
  ASSERT_TRUE(frame.has_value());
  frame->clear(Colour{.red = 0.0F, .green = 1.0F, .blue = 0.0F, .alpha = 1.0F});
  ASSERT_TRUE(frame->present().has_value());
  const auto image = gpu->capture();
  ASSERT_TRUE(image.has_value());
  EXPECT_EQ(image->width(), 64U);
  EXPECT_EQ(image->height(), 48U);
  EXPECT_TRUE(
      conformance::GpuSuite::matches(*image->pixel(10, 10), {0, 255, 0, 255}));
  EXPECT_EQ(gpu->statistics().frames_presented, 1U);
}

TEST_F(VulkanGpuTest, AFramePlannedAtANewExtentResizesTheOffscreenTarget) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  constexpr SurfaceSize wide{.width = SurfaceWidth{96},
                             .height = SurfaceHeight{72}};
  const Colour green{.red = 0.0F, .green = 1.0F, .blue = 0.0F, .alpha = 1.0F};
  const Colour red{.red = 1.0F, .green = 0.0F, .blue = 0.0F, .alpha = 1.0F};
  const auto clear_at = [&gpu](SurfaceSize extent, Colour colour) {
    auto frame = gpu->begin_frame();
    ASSERT_TRUE(frame.has_value());
    frame->plan(FramePlan{.predicted_present = std::chrono::nanoseconds{0},
                          .replay = false,
                          .exclusive = false,
                          .extent = extent});
    frame->clear(colour);
    ASSERT_TRUE(frame->present().has_value());
  };
  clear_at(small, green);
  const auto first = gpu->capture();
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->width(), 64U);
  EXPECT_EQ(first->height(), 48U);
  const std::uint64_t settled =
      gpu->device().memory_statistics().allocations_live;
  clear_at(wide, red);
  const auto resized = gpu->capture();
  ASSERT_TRUE(resized.has_value());
  EXPECT_EQ(resized->width(), 96U);
  EXPECT_EQ(resized->height(), 72U);
  EXPECT_TRUE(conformance::GpuSuite::matches(*resized->pixel(90, 70),
                                             {255, 0, 0, 255}));
  for (int round = 0; round < 8; ++round) {
    clear_at(wide, red);
    clear_at(small, green);
  }
  const auto back = gpu->capture();
  ASSERT_TRUE(back.has_value());
  EXPECT_EQ(back->width(), 64U);
  EXPECT_EQ(back->height(), 48U);
  EXPECT_TRUE(
      conformance::GpuSuite::matches(*back->pixel(60, 40), {0, 255, 0, 255}));
  EXPECT_EQ(gpu->device().memory_statistics().allocations_live, settled);
  EXPECT_EQ(gpu->statistics().frames_presented, 18U);
}

TEST_F(VulkanGpuTest, DiscardedFramesLeaveTheLastPresentedImage) {
  auto gpu = offscreen({.size = small, .images = 3});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  {
    auto first = gpu->begin_frame();
    ASSERT_TRUE(first.has_value());
    first->clear(Colour{.red = 1.0F});
    ASSERT_TRUE(first->present().has_value());
  }
  {
    auto discarded = gpu->begin_frame();
    ASSERT_TRUE(discarded.has_value());
    discarded->clear(Colour{.blue = 1.0F});
    EXPECT_FALSE(gpu->begin_frame().has_value());
  }
  EXPECT_TRUE(
      conformance::GpuSuite::matches(pixel(*gpu, 5, 5), {255, 0, 0, 255}));
}

TEST_F(VulkanGpuTest, RendersIntoATargetAndSamplesItInTheSameFrame) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  auto green = solid(*gpu, {0x00, 0xff, 0x00, 0xff});
  ASSERT_TRUE(green.has_value());
  auto target = gpu->create_texture({.width = 64,
                                     .height = 64,
                                     .format = TextureFormat::rgba8,
                                     .usage = TextureUsage::render_target});
  ASSERT_TRUE(target.has_value());
  auto sampled = gpu->create_texture({.width = 4, .height = 4});
  ASSERT_TRUE(sampled.has_value());
  for (int round = 0; round < 3; ++round) {
    auto frame = gpu->begin_frame();
    ASSERT_TRUE(frame.has_value());
    frame->clear(Colour{.alpha = 1.0F});
    auto geometry = frame->allocate_shapes(8, 12);
    ASSERT_TRUE(geometry.has_value());
    const auto into = shape(100.0F, 0.0F, 132.0F, 64.0F, white_colour);
    const auto out = shape(0.0F, 0.0F, 64.0F, 48.0F, white_colour);
    std::ranges::copy(into, geometry->vertices.begin());
    std::ranges::copy(out, geometry->vertices.begin() + 4);
    const std::array<Index, 12> indices{0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
    std::ranges::copy(indices, geometry->indices.begin());
    const std::array pass_commands{DrawCommand{
        .texture = green->id(),
        .clip = ClipRect{.x = 100, .y = 0, .width = 64, .height = 64},
        .index_offset = 0,
        .index_count = 6,
        .vertex_offset = 0}};
    const RenderPass pass{
        .target = target->id(),
        .region =
            PassRegion{.x = 100.0F, .y = 0.0F, .width = 64.0F, .height = 64.0F},
        .clear = Colour{.red = 1.0F, .alpha = 1.0F}};
    ASSERT_TRUE(frame
                    ->draw(ShapeDrawData{.geometry = *geometry,
                                         .commands = pass_commands},
                           pass)
                    .has_value());
    const auto refused = frame->draw(
        ShapeDrawData{.geometry = *geometry, .commands = pass_commands},
        RenderPass{.target = sampled->id(), .region = pass.region});
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code(), ProviderErrorCode::invalid_argument);
    const std::array screen_commands{
        DrawCommand{.texture = target->id(),
                    .clip = ClipRect{.x = 0, .y = 0, .width = 64, .height = 48},
                    .index_offset = 0,
                    .index_count = 6,
                    .vertex_offset = 4}};
    ASSERT_TRUE(frame
                    ->draw(ShapeDrawData{.geometry = *geometry,
                                         .commands = screen_commands})
                    .has_value());
    ASSERT_TRUE(frame->present().has_value());
    EXPECT_TRUE(
        conformance::GpuSuite::matches(pixel(*gpu, 8, 24), {0, 255, 0, 255}))
        << round;
    EXPECT_TRUE(
        conformance::GpuSuite::matches(pixel(*gpu, 56, 24), {255, 0, 0, 255}))
        << round;
  }
  EXPECT_EQ(gpu->statistics().offscreen_passes, 3U);
}

TEST_F(VulkanGpuTest, TimestampsTimePassesWithoutWaitingOnResults) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  if (gpu->device().capabilities().timestamp_bits == 0) {
    GTEST_SKIP() << "the device has no timestamps on its graphics queue";
  }
  auto green = solid(*gpu, {0x00, 0xff, 0x00, 0xff});
  ASSERT_TRUE(green.has_value());
  auto target = gpu->create_texture({.width = 1024,
                                     .height = 1024,
                                     .format = TextureFormat::rgba8,
                                     .usage = TextureUsage::render_target});
  ASSERT_TRUE(target.has_value());
  const auto settle = [&gpu] {
    const GpuSerial serial{gpu->statistics().frames_presented};
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!gpu->completed(serial) &&
           std::chrono::steady_clock::now() < deadline) {
      std::this_thread::yield();
    }
    return gpu->completed(serial);
  };
  const auto render = [&](bool effects) {
    auto frame = gpu->begin_frame();
    ASSERT_TRUE(frame.has_value());
    frame->clear(Colour{.alpha = 1.0F});
    auto geometry = frame->allocate_shapes(4, 6);
    ASSERT_TRUE(geometry.has_value());
    auto vertices = shape(0.0F, 0.0F, 1024.0F, 1024.0F, white_colour);
    for (ShapeVertex& vertex : vertices) {
      vertex.kind = ShapeKind::blur_down;
    }
    std::ranges::copy(vertices, geometry->vertices.begin());
    const std::array<Index, 6> indices{0, 1, 2, 0, 2, 3};
    std::ranges::copy(indices, geometry->indices.begin());
    const std::array commands{DrawCommand{
        .texture = green->id(),
        .clip = ClipRect{.x = 0, .y = 0, .width = 1024, .height = 1024},
        .index_offset = 0,
        .index_count = 6,
        .vertex_offset = 0}};
    if (effects) {
      for (int pass = 0; pass < 8; ++pass) {
        ASSERT_TRUE(
            frame
                ->draw(
                    ShapeDrawData{.geometry = *geometry, .commands = commands},
                    RenderPass{.target = target->id(),
                               .region = PassRegion{.width = 1024.0F,
                                                    .height = 1024.0F},
                               .clear = std::nullopt})
                .has_value());
      }
    }
    ASSERT_TRUE(frame->present().has_value());
  };
  for (int index = 0; index < 4; ++index) {
    render(true);
    ASSERT_TRUE(settle());
  }
  const GpuTimings glass = gpu->timings();
  std::println(
      "timestamps with passes: effects {} ns, interface {} ns, {} "
      "passes",
      glass.effects.value_or(std::chrono::nanoseconds{-1}).count(),
      glass.interface.value_or(std::chrono::nanoseconds{-1}).count(),
      glass.passes);
  EXPECT_TRUE(glass.measured_on_gpu);
  ASSERT_TRUE(glass.effects.has_value());
  EXPECT_GT(glass.effects->count(), 0);
  EXPECT_EQ(glass.passes, 8U);
  for (int index = 0; index < 3; ++index) {
    render(false);
    ASSERT_TRUE(settle());
  }
  const GpuTimings idle = gpu->timings();
  std::println("timestamps without passes: effects {}, interface {} ns",
               idle.effects.has_value() ? "measured" : "none",
               idle.interface.value_or(std::chrono::nanoseconds{-1}).count());
  EXPECT_FALSE(idle.effects.has_value());
  EXPECT_TRUE(idle.interface.has_value());
  EXPECT_EQ(idle.passes, 0U);
  const VulkanGpu::Statistics statistics = gpu->statistics();
  EXPECT_EQ(statistics.fence_waits, 0U);
  EXPECT_EQ(statistics.timestamp_stalls, 0U);
  EXPECT_EQ(statistics.timestamp_reads, 7U);
}

TEST_F(VulkanGpuTest, SubAllocationsKeepAlignmentAndSeparateImagesFromBuffers) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  const VulkanDevice& device = gpu->device();
  const MemoryAllocator::Statistics before = device.memory_statistics();
  std::vector<VulkanBuffer> buffers;
  std::vector<VulkanImage> images;
  for (std::uint32_t index = 0; index < 96; ++index) {
    auto buffer = VulkanBuffer::create(device, 1000 + (index * 37),
                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                       MemoryUse::upload);
    ASSERT_TRUE(buffer.has_value());
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device.handle(), buffer->handle(),
                                  &requirements);
    EXPECT_EQ(buffer->allocation().offset() % requirements.alignment, 0U);
    EXPECT_GE(buffer->allocation().size(), requirements.size);
    buffers.push_back(std::move(*buffer));
    auto image = VulkanImage::create(
        device, VulkanImage::Description{.width = 3 + index,
                                         .height = 5,
                                         .format = VK_FORMAT_R8G8B8A8_UNORM,
                                         .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
                                         .swizzle = {}});
    ASSERT_TRUE(image.has_value());
    vkGetImageMemoryRequirements(device.handle(), image->image(),
                                 &requirements);
    EXPECT_EQ(image->allocation().offset() % requirements.alignment, 0U);
    images.push_back(std::move(*image));
  }
  const VkDeviceSize granularity =
      device.properties().limits.bufferImageGranularity;
  for (const VulkanBuffer& buffer : buffers) {
    for (const VulkanImage& image : images) {
      const MemoryAllocator::Allocation& linear = buffer.allocation();
      const MemoryAllocator::Allocation& optimal = image.allocation();
      if (linear.memory() != optimal.memory()) {
        continue;
      }
      const VkDeviceSize linear_page =
          (linear.offset() + linear.size() - 1) / granularity;
      const VkDeviceSize optimal_page = optimal.offset() / granularity;
      EXPECT_NE(linear_page, optimal_page);
    }
  }
  for (std::size_t index = 0; index < buffers.size(); ++index) {
    for (std::size_t other = index + 1; other < buffers.size(); ++other) {
      const MemoryAllocator::Allocation& left = buffers[index].allocation();
      const MemoryAllocator::Allocation& right = buffers[other].allocation();
      if (left.memory() == right.memory()) {
        EXPECT_TRUE(left.offset() + left.size() <= right.offset() ||
                    right.offset() + right.size() <= left.offset());
      }
    }
  }
  const MemoryAllocator::Statistics during = device.memory_statistics();
  std::println("192 resources: {} memory blocks created, {} sub-allocations",
               during.blocks_created - before.blocks_created,
               during.allocations_made - before.allocations_made);
  EXPECT_EQ(during.allocations_made - before.allocations_made, 192U);
  EXPECT_LE(during.blocks_created - before.blocks_created, 4U);
  const std::uint64_t block = buffers[40].allocation().block();
  const VkDeviceSize offset = buffers[40].allocation().offset();
  buffers.erase(buffers.begin() + 40);
  {
    auto reused = VulkanBuffer::create(device, 1000 + (40 * 37),
                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                       MemoryUse::upload);
    ASSERT_TRUE(reused.has_value());
    EXPECT_EQ(reused->allocation().block(), block);
    EXPECT_EQ(reused->allocation().offset(), offset);
  }
  const std::uint64_t created = device.memory_statistics().blocks_created;
  buffers.clear();
  images.clear();
  EXPECT_EQ(device.memory_statistics().allocations_live,
            before.allocations_live);
  EXPECT_EQ(device.memory_statistics().blocks_created, created);
}

TEST_F(VulkanGpuTest,
       RetiredTexturesReturnTheirMemoryOnceTheirSerialCompletes) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  const VulkanDevice& device = gpu->device();
  auto kept = solid(*gpu, {0x10, 0x20, 0x30, 0xff});
  ASSERT_TRUE(kept.has_value());
  auto created = gpu->create_texture({.width = 64,
                                      .height = 64,
                                      .format = TextureFormat::rgba8,
                                      .usage = TextureUsage::render_target});
  ASSERT_TRUE(created.has_value());
  std::optional<Texture> retired{std::move(*created)};
  show(*gpu, retired->id(), Colour{.alpha = 1.0F});
  const std::uint64_t live = device.memory_statistics().allocations_live;
  retired.reset();
  {
    auto frame = gpu->begin_frame();
    ASSERT_TRUE(frame.has_value());
    frame->clear(Colour{.alpha = 1.0F});
    ASSERT_TRUE(frame->present().has_value());
  }
  EXPECT_EQ(device.memory_statistics().allocations_live, live);
  const GpuSerial serial{gpu->statistics().frames_presented};
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds{5};
  while (!gpu->completed(serial) &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::yield();
  }
  ASSERT_TRUE(gpu->completed(serial));
  EXPECT_EQ(device.memory_statistics().allocations_live, live - 1);
  const std::uint64_t blocks = device.memory_statistics().blocks_created;
  auto replacement =
      gpu->create_texture({.width = 64,
                           .height = 64,
                           .format = TextureFormat::rgba8,
                           .usage = TextureUsage::render_target});
  ASSERT_TRUE(replacement.has_value());
  EXPECT_EQ(device.memory_statistics().allocations_live, live);
  EXPECT_EQ(device.memory_statistics().blocks_created, blocks);
}

TEST_F(VulkanGpuTest, PartialTextureUpdatesKeepTheRestOfTheTexture) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  auto texture = gpu->create_texture({.width = 2,
                                      .height = 1,
                                      .format = TextureFormat::rgba8,
                                      .filter = SamplerFilter::nearest});
  ASSERT_TRUE(texture.has_value());
  const std::array red{std::byte{0xff}, std::byte{0x00}, std::byte{0x00},
                       std::byte{0xff}};
  const std::array green{std::byte{0x00}, std::byte{0xff}, std::byte{0x00},
                         std::byte{0xff}};
  ASSERT_TRUE(gpu->update_texture(
                     *texture, {.x = 0, .y = 0, .width = 1, .height = 1}, red)
                  .has_value());
  show(*gpu, texture->id(), Colour{.alpha = 1.0F});
  EXPECT_TRUE(
      conformance::GpuSuite::matches(pixel(*gpu, 16, 24), {255, 0, 0, 255}));
  EXPECT_TRUE(
      conformance::GpuSuite::matches(pixel(*gpu, 48, 24), {0, 0, 0, 255}));
  const std::uint64_t uploaded = gpu->statistics().texture_bytes_uploaded;
  ASSERT_TRUE(gpu->update_texture(
                     *texture, {.x = 1, .y = 0, .width = 1, .height = 1}, green)
                  .has_value());
  EXPECT_EQ(gpu->statistics().texture_bytes_uploaded - uploaded, 4U);
  show(*gpu, texture->id(), Colour{.alpha = 1.0F});
  EXPECT_TRUE(
      conformance::GpuSuite::matches(pixel(*gpu, 16, 24), {255, 0, 0, 255}));
  EXPECT_TRUE(
      conformance::GpuSuite::matches(pixel(*gpu, 48, 24), {0, 255, 0, 255}));
  const std::vector<std::byte> strided{std::byte{0x00}, std::byte{0x00},
                                       std::byte{0xff}, std::byte{0xff},
                                       std::byte{0x11}, std::byte{0x22}};
  ASSERT_TRUE(gpu->update_texture(*texture,
                                  {.x = 1, .y = 0, .width = 1, .height = 1},
                                  PixelRows::strided(strided, 6))
                  .has_value());
  show(*gpu, texture->id(), Colour{.alpha = 1.0F});
  EXPECT_TRUE(
      conformance::GpuSuite::matches(pixel(*gpu, 48, 24), {0, 0, 255, 255}));
}

TEST_F(VulkanGpuTest, NearestAndLinearSamplersFilterDifferently) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  const std::array<std::byte, 8> texels{
      std::byte{0xff}, std::byte{0x00}, std::byte{0x00}, std::byte{0xff},
      std::byte{0x00}, std::byte{0x00}, std::byte{0xff}, std::byte{0xff}};
  auto nearest = gpu->create_texture(
      {.width = 2, .height = 1, .filter = SamplerFilter::nearest});
  auto linear = gpu->create_texture(
      {.width = 2, .height = 1, .filter = SamplerFilter::linear});
  ASSERT_TRUE(nearest.has_value());
  ASSERT_TRUE(linear.has_value());
  ASSERT_TRUE(gpu->update_texture(*nearest,
                                  {.x = 0, .y = 0, .width = 2, .height = 1},
                                  texels)
                  .has_value());
  ASSERT_TRUE(gpu->update_texture(
                     *linear, {.x = 0, .y = 0, .width = 2, .height = 1}, texels)
                  .has_value());
  show(*gpu, nearest->id());
  EXPECT_TRUE(
      conformance::GpuSuite::matches(pixel(*gpu, 30, 24), {255, 0, 0, 255}));
  EXPECT_TRUE(
      conformance::GpuSuite::matches(pixel(*gpu, 33, 24), {0, 0, 255, 255}));
  show(*gpu, linear->id());
  const auto blended = pixel(*gpu, 30, 24);
  EXPECT_GT(blended[0], 64);
  EXPECT_GT(blended[2], 64);
}

TEST_F(VulkanGpuTest, EveryTextureFormatSamplesInItsOwnChannelOrder) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  struct Case {
    TextureFormat format;
    std::vector<std::byte> texel;
    std::array<std::uint8_t, 4> expected;
  };
  const std::vector<Case> cases{
      {TextureFormat::rgba8,
       {std::byte{0xff}, std::byte{0x00}, std::byte{0x00}, std::byte{0xff}},
       {255, 0, 0, 255}},
      {TextureFormat::bgra8,
       {std::byte{0xff}, std::byte{0x00}, std::byte{0x00}, std::byte{0xff}},
       {0, 0, 255, 255}},
      {TextureFormat::rgb565,
       {std::byte{0x1f}, std::byte{0x00}},
       {0, 0, 255, 255}},
      {TextureFormat::rgb565,
       {std::byte{0x00}, std::byte{0xf8}},
       {255, 0, 0, 255}},
      {TextureFormat::alpha8, {std::byte{0x80}}, {128, 128, 128, 255}},
      {TextureFormat::r8, {std::byte{0x80}}, {128, 0, 0, 255}},
      {TextureFormat::rg8,
       {std::byte{0x40}, std::byte{0x80}},
       {64, 128, 0, 255}},
  };
  for (const Case& each : cases) {
    auto texture =
        gpu->create_texture({.width = 1, .height = 1, .format = each.format});
    ASSERT_TRUE(texture.has_value());
    ASSERT_TRUE(gpu->update_texture(*texture,
                                    {.x = 0, .y = 0, .width = 1, .height = 1},
                                    each.texel)
                    .has_value());
    show(*gpu, texture->id(), Colour{.alpha = 1.0F});
    EXPECT_TRUE(
        conformance::GpuSuite::matches(pixel(*gpu, 10, 10), each.expected))
        << static_cast<int>(each.format);
  }
}

TEST_F(VulkanGpuTest, ResidentArenaCopiesOnlyTouchedRanges) {
  auto gpu = offscreen({.size = small, .images = 3});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  auto white = gpu->create_texture({.width = 1, .height = 1});
  ASSERT_TRUE(white.has_value());
  const std::array<std::byte, 4> opaque{std::byte{0xff}, std::byte{0xff},
                                        std::byte{0xff}, std::byte{0xff}};
  ASSERT_TRUE(gpu->update_texture(
                     *white, {.x = 0, .y = 0, .width = 1, .height = 1}, opaque)
                  .has_value());
  constexpr std::uint32_t quads = 4;
  constexpr std::size_t quad_bytes = 4 * sizeof(ShapeVertex);
  const std::array colours{
      Vertex::rgba(255, 0, 0, 255), Vertex::rgba(0, 255, 0, 255),
      Vertex::rgba(0, 0, 255, 255), Vertex::rgba(255, 255, 0, 255)};
  const std::array commands{
      DrawCommand{.texture = white->id(),
                  .clip = ClipRect{.x = 0, .y = 0, .width = 64, .height = 48},
                  .index_offset = 0,
                  .index_count = quads * 6,
                  .vertex_offset = 0}};
  const auto frame =
      [&](std::optional<std::pair<std::uint32_t, std::uint32_t>> change) {
        auto begun = gpu->begin_frame();
        ASSERT_TRUE(begun.has_value());
        begun->clear(Colour{.alpha = 1.0F});
        auto resident = begun->allocate_resident(quads * 4, quads * 6);
        ASSERT_TRUE(resident.has_value());
        EXPECT_EQ(resident->generation, 1U);
        if (change.has_value()) {
          const auto [index, colour] = *change;
          const auto vertices =
              shape(static_cast<float>(index) * 16.0F, 0.0F,
                    static_cast<float>(index + 1) * 16.0F, 48.0F, colour);
          std::ranges::copy(
              vertices, resident->geometry.vertices.subspan(index * 4).begin());
          begun->touch(VertexRange{.first = index * 4, .count = 4});
        }
        for (std::uint32_t index = 0; index < quads; ++index) {
          const std::array<Index, 6> each{index * 4,       (index * 4) + 1,
                                          (index * 4) + 2, index * 4,
                                          (index * 4) + 2, (index * 4) + 3};
          std::ranges::copy(
              each, resident->geometry.indices.subspan(index * 6).begin());
        }
        ASSERT_TRUE(begun
                        ->draw(ShapeDrawData{.geometry = resident->geometry,
                                             .commands = commands})
                        .has_value());
        ASSERT_TRUE(begun->present().has_value());
      };
  const auto copied = [&gpu]() {
    return gpu->statistics().vertex_bytes_copied;
  };
  {
    auto begun = gpu->begin_frame();
    ASSERT_TRUE(begun.has_value());
    auto resident = begun->allocate_resident(quads * 4, quads * 6);
    ASSERT_TRUE(resident.has_value());
    for (std::uint32_t index = 0; index < quads; ++index) {
      const auto vertices =
          shape(static_cast<float>(index) * 16.0F, 0.0F,
                static_cast<float>(index + 1) * 16.0F, 48.0F, colours[index]);
      std::ranges::copy(vertices,
                        resident->geometry.vertices.subspan(index * 4).begin());
    }
    begun->touch(VertexRange{.first = 0, .count = quads * 4});
  }
  std::vector<std::uint64_t> per_frame;
  for (int index = 0; index < 6; ++index) {
    const std::uint64_t before = copied();
    frame(std::nullopt);
    per_frame.push_back(copied() - before);
  }
  const std::uint64_t whole = quads * quad_bytes;
  EXPECT_EQ(per_frame,
            (std::vector<std::uint64_t>{whole, whole, whole, 0, 0, 0}));
  EXPECT_TRUE(
      conformance::GpuSuite::matches(pixel(*gpu, 40, 24), {0, 0, 255, 255}));
  per_frame.clear();
  for (int index = 0; index < 5; ++index) {
    const std::uint64_t before = copied();
    frame(index == 0
              ? std::optional{std::pair{2U, Vertex::rgba(255, 0, 255, 255)}}
              : std::nullopt);
    per_frame.push_back(copied() - before);
    EXPECT_TRUE(
        conformance::GpuSuite::matches(pixel(*gpu, 40, 24), {255, 0, 255, 255}))
        << index;
    EXPECT_TRUE(
        conformance::GpuSuite::matches(pixel(*gpu, 8, 24), {255, 0, 0, 255}))
        << index;
  }
  EXPECT_EQ(per_frame, (std::vector<std::uint64_t>{quad_bytes, quad_bytes,
                                                   quad_bytes, 0, 0}));
}

TEST_F(VulkanGpuTest, SlotReuseNeverWaitsWhenTheGpuIsAhead) {
  auto gpu = offscreen({.size = small, .images = 3});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  for (int index = 0; index < 12; ++index) {
    auto frame = gpu->begin_frame();
    ASSERT_TRUE(frame.has_value());
    frame->clear(Colour{.red = 0.5F});
    ASSERT_TRUE(frame->present().has_value());
    const GpuSerial serial{gpu->statistics().frames_presented};
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!gpu->completed(serial) &&
           std::chrono::steady_clock::now() < deadline) {
      std::this_thread::yield();
    }
    ASSERT_TRUE(gpu->completed(serial));
  }
  EXPECT_EQ(gpu->statistics().fence_waits, 0U);
  EXPECT_EQ(gpu->statistics().frames_presented, 12U);
}

TEST_F(VulkanGpuTest, TexturesMayOutliveTheGpu) {
  std::optional<Texture> texture;
  std::optional<GpuFrame> frame;
  {
    auto gpu = offscreen({.size = small, .images = 2});
    if (!gpu.has_value()) {
      GTEST_SKIP() << skip_reason(gpu.error());
    }
    auto created = gpu->create_texture(
        {.width = 4, .height = 4, .format = TextureFormat::alpha8});
    ASSERT_TRUE(created.has_value());
    const std::vector<std::byte> pixels(16, std::byte{0x7f});
    ASSERT_TRUE(gpu->update_texture(*created,
                                    {.x = 0, .y = 0, .width = 4, .height = 4},
                                    pixels)
                    .has_value());
    EXPECT_EQ(gpu->live_textures(), 1U);
    EXPECT_GE(gpu->resources().textures_created, 1U);
    EXPECT_GT(gpu->resources().memory_blocks_created, 0U);
    texture.emplace(std::move(*created));
    auto begun = gpu->begin_frame();
    ASSERT_TRUE(begun.has_value());
    frame.emplace(std::move(*begun));
  }
  EXPECT_TRUE(texture->valid());
  const auto presented = frame->present();
  ASSERT_FALSE(presented.has_value());
  EXPECT_EQ(presented.error().code(), ProviderErrorCode::platform_failure);
  texture.reset();
}

TEST_F(VulkanGpuTest, ManyTexturesEachGetADescriptor) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  std::vector<Texture> textures;
  for (std::uint32_t index = 0; index < 600; ++index) {
    auto texture = gpu->create_texture({.width = 1, .height = 1});
    ASSERT_TRUE(texture.has_value()) << index;
    const std::array<std::byte, 4> texel{
        std::byte{static_cast<std::uint8_t>(index & 0xffU)}, std::byte{0x00},
        std::byte{0x80}, std::byte{0xff}};
    ASSERT_TRUE(gpu->update_texture(*texture,
                                    {.x = 0, .y = 0, .width = 1, .height = 1},
                                    texel)
                    .has_value());
    textures.push_back(std::move(*texture));
  }
  show(*gpu, textures[599].id(), Colour{.alpha = 1.0F});
  EXPECT_TRUE(conformance::GpuSuite::matches(pixel(*gpu, 10, 10),
                                             {599 & 0xff, 0, 128, 255}));
  textures.erase(textures.begin(), textures.begin() + 300);
  for (int index = 0; index < 4; ++index) {
    show(*gpu, textures.back().id(), Colour{.alpha = 1.0F});
  }
  EXPECT_EQ(gpu->live_textures(), 300U);
}

TEST_F(VulkanGpuTest, HalfRateFramesRecordAPresentIntervalOfTwo) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  const auto present = [&gpu](std::uint32_t interval) {
    auto frame = gpu->begin_frame();
    ASSERT_TRUE(frame.has_value());
    frame->plan(FramePlan{.predicted_present = {},
                          .replay = false,
                          .exclusive = false,
                          .extent = {},
                          .swap_interval = interval,
                          .capture = false});
    frame->clear(
        Colour{.red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 1.0F});
    ASSERT_TRUE(frame->present().has_value());
  };
  present(1);
  EXPECT_EQ(gpu->statistics().present_interval, 1U);
  EXPECT_EQ(gpu->statistics().interval_changes, 0U);
  present(2);
  present(2);
  EXPECT_EQ(gpu->statistics().present_interval, 2U);
  EXPECT_EQ(gpu->statistics().interval_changes, 1U);
  present(1);
  EXPECT_EQ(gpu->statistics().present_interval, 1U);
  EXPECT_EQ(gpu->statistics().interval_changes, 2U);
}

TEST_F(VulkanGpuTest, ReplayDrawsTheLastFramesVerticesWithItsOwnCommands) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  auto white = solid(*gpu, {255, 255, 255, 255});
  ASSERT_TRUE(white.has_value());
  const std::uint32_t colour = Vertex::rgba(255, 255, 255, 255);
  const auto vertices = shape(0.0F, 0.0F, 16.0F, 16.0F, colour);
  const std::array<Index, 6> indices{0, 1, 2, 0, 2, 3};
  const DrawCommand command{
      .texture = white->id(),
      .clip = ClipRect{.x = 0, .y = 0, .width = 64, .height = 48},
      .index_offset = 0,
      .index_count = 6,
      .vertex_offset = 0};
  const Colour black{.red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 1.0F};
  {
    auto frame = gpu->begin_frame();
    ASSERT_TRUE(frame.has_value());
    frame->clear(black);
    auto resident = frame->allocate_resident(4, 6);
    ASSERT_TRUE(resident.has_value());
    std::ranges::copy(vertices, resident->geometry.vertices.begin());
    std::ranges::copy(indices, resident->geometry.indices.begin());
    frame->touch(VertexRange{.first = 0, .count = 4});
    const std::array commands{command};
    ASSERT_TRUE(frame
                    ->draw(ShapeDrawData{.geometry = resident->geometry,
                                         .commands = commands})
                    .has_value());
    ASSERT_TRUE(frame->present().has_value());
  }
  EXPECT_GT(pixel(*gpu, 8, 8)[0], 200);
  EXPECT_LT(pixel(*gpu, 40, 8)[0], 50);
  auto replay = gpu->begin_replay();
  ASSERT_TRUE(replay.has_value());
  replay->plan(FramePlan{.predicted_present = {},
                         .replay = true,
                         .exclusive = false,
                         .extent = {},
                         .swap_interval = 1,
                         .capture = false});
  replay->clear(black);
  DrawCommand moved = command;
  moved.translation = {32.0F, 0.0F};
  const std::array replayed{moved};
  ASSERT_TRUE(replay
                  ->replay(ReplayDrawData{.indices = indices,
                                          .commands = replayed,
                                          .vertex_count = 4,
                                          .generation = 1})
                  .has_value());
  ASSERT_TRUE(replay->present().has_value());
  EXPECT_LT(pixel(*gpu, 8, 8)[0], 50);
  EXPECT_GT(pixel(*gpu, 40, 8)[0], 200);
  EXPECT_EQ(gpu->statistics().replays, 1U);
}

TEST_F(VulkanGpuTest, UiAllocationsNeverWaitForALongRecording) {
  static constexpr std::chrono::milliseconds recording{80};
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  std::atomic<bool> inside{false};
  const FrameHook hook = gpu->add_frame_hook(
      FrameStage::before_ui,
      [&inside](VkCommandBuffer, const VulkanGpu::FrameTarget&,
                const FrameInfo&) {
        inside.store(true);
        std::this_thread::sleep_for(recording);
        inside.store(false);
      });
  auto first = gpu->begin_frame();
  ASSERT_TRUE(first.has_value());
  first->clear(Colour{.red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 1.0F});
  auto sealed = first->seal();
  ASSERT_TRUE(sealed.has_value());
  standard::StdThreadingProvider threading;
  std::optional<FramePacket> packet{std::move(*sealed)};
  auto submitter = threading.start(
      ThreadSpec{.name = "test-render", .role = ThreadRole::render},
      [&packet](const std::stop_token&) {
        static_cast<void>(packet->submit());
      });
  ASSERT_TRUE(submitter.has_value());
  std::optional<Thread> rendering{std::move(*submitter)};
  while (!inside.load()) {
    std::this_thread::yield();
  }
  const std::uint64_t waits = gpu->statistics().allocation_waits;
  const auto started = std::chrono::steady_clock::now();
  auto second = gpu->begin_frame();
  ASSERT_TRUE(second.has_value());
  auto resident = second->allocate_resident(64, 96);
  EXPECT_TRUE(resident.has_value());
  auto shapes = second->allocate_shapes(16, 24);
  EXPECT_TRUE(shapes.has_value());
  auto texture = solid(*gpu, {10, 20, 30, 255});
  EXPECT_TRUE(texture.has_value());
  const auto elapsed = std::chrono::steady_clock::now() - started;
  const bool overlapped = inside.load();
  rendering->request_stop();
  rendering.reset();
  EXPECT_TRUE(overlapped);
  EXPECT_LT(elapsed, recording / 2);
  EXPECT_EQ(gpu->statistics().allocation_waits, waits);
  ASSERT_TRUE(second->present().has_value());
}

}
