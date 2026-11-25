#include <arisa/components/vstack.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/provider/default_factory.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/gpu/frame_source.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/gpu/vertex.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/providers.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/provider/vulkan/device_object.hpp>
#include <arisa/provider/vulkan/offscreen_target.hpp>
#include <arisa/provider/vulkan/vulkan_buffer.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>
#include <arisa/provider/vulkan/vulkan_gpu.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/embedded_root.hpp>
#include <arisa/runtime/root.hpp>
#include <arisa/theme/colour.hpp>

#include "vulkan_test.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace arisa::provider::vulkan {

class VulkanOverlayTest : public VulkanTest {
 protected:
  static constexpr std::uint32_t width = 64;
  static constexpr std::uint32_t height = 48;
  static constexpr SurfaceSize extent{.width = SurfaceWidth{width},
                                      .height = SurfaceHeight{height}};

  class Dock final : public composition::Component {
   public:
    explicit Dock(const composition::Observable<int>& shade) noexcept
        : shade_(shade) {}

    auto operator==(const Dock& other) const -> bool {
      return &shade_.get() == &other.shade_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto level =
          static_cast<std::uint32_t>(0xC0 + (context.read(shade_.get()) % 32));
      return components::VStack(
                 components::VStack()
                     .frame({.width = 24.0F, .height = 16.0F})
                     .background(theme::Colour::hex(level << 16U)))
          .frame({.width = 64.0F,
                  .height = 48.0F,
                  .alignment = composition::Alignment::top_leading});
    }

   private:
    std::reference_wrapper<const composition::Observable<int>> shade_;
  };

  class Commands {
   public:
    [[nodiscard]] static auto create(const VulkanDevice& device)
        -> std::optional<Commands> {
      VkDevice const handle = device.handle();
      const VkCommandPoolCreateInfo pool_info{
          .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .queueFamilyIndex = device.queue_family()};
      VkCommandPool pool = VK_NULL_HANDLE;
      if (vkCreateCommandPool(handle, &pool_info, nullptr, &pool) !=
          VK_SUCCESS) {
        return std::nullopt;
      }
      VulkanCommandPoolHandle owned{{.device = handle, .object = pool}};
      const VkCommandBufferAllocateInfo buffer_info{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          .pNext = nullptr,
          .commandPool = pool,
          .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          .commandBufferCount = 1};
      VkCommandBuffer buffer = VK_NULL_HANDLE;
      if (vkAllocateCommandBuffers(handle, &buffer_info, &buffer) !=
          VK_SUCCESS) {
        return std::nullopt;
      }
      return Commands{std::move(owned), buffer};
    }

    auto begin() -> bool {
      if (vkResetCommandPool(pool_.get().device, pool_.get().object, 0) !=
          VK_SUCCESS) {
        return false;
      }
      const VkCommandBufferBeginInfo info{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
          .pNext = nullptr,
          .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
          .pInheritanceInfo = nullptr};
      return vkBeginCommandBuffer(buffer_, &info) == VK_SUCCESS;
    }

    void transition(VkImage image, VkImageLayout from, VkImageLayout to,
                    VkAccessFlags written, VkAccessFlags read) const {
      const VkImageMemoryBarrier barrier{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .pNext = nullptr,
          .srcAccessMask = written,
          .dstAccessMask = read,
          .oldLayout = from,
          .newLayout = to,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = image,
          .subresourceRange =
              VkImageSubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                      .baseMipLevel = 0,
                                      .levelCount = 1,
                                      .baseArrayLayer = 0,
                                      .layerCount = 1}};
      vkCmdPipelineBarrier(buffer_, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                           VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                           nullptr, 1, &barrier);
    }

    void read_back(VkImage image, VkBuffer into) const {
      const VkBufferImageCopy copy{
          .bufferOffset = 0,
          .bufferRowLength = 0,
          .bufferImageHeight = 0,
          .imageSubresource =
              VkImageSubresourceLayers{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                       .mipLevel = 0,
                                       .baseArrayLayer = 0,
                                       .layerCount = 1},
          .imageOffset = VkOffset3D{.x = 0, .y = 0, .z = 0},
          .imageExtent =
              VkExtent3D{.width = width, .height = height, .depth = 1}};
      vkCmdCopyImageToBuffer(
          buffer_, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, into, 1, &copy);
    }

    auto submit(const VulkanDevice& device,
                std::optional<VulkanGpu::OverlayFence> fence) const -> bool {
      if (vkEndCommandBuffer(buffer_) != VK_SUCCESS) {
        return false;
      }
      const std::uint64_t value = fence.has_value() ? fence->value : 0;
      const VkSemaphore semaphore =
          fence.has_value() ? fence->semaphore : VK_NULL_HANDLE;
      const VkTimelineSemaphoreSubmitInfo values{
          .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
          .pNext = nullptr,
          .waitSemaphoreValueCount = 0,
          .pWaitSemaphoreValues = nullptr,
          .signalSemaphoreValueCount = fence.has_value() ? 1U : 0U,
          .pSignalSemaphoreValues = &value};
      const VkSubmitInfo info{
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
          .pNext = fence.has_value() ? &values : nullptr,
          .waitSemaphoreCount = 0,
          .pWaitSemaphores = nullptr,
          .pWaitDstStageMask = nullptr,
          .commandBufferCount = 1,
          .pCommandBuffers = &buffer_,
          .signalSemaphoreCount = fence.has_value() ? 1U : 0U,
          .pSignalSemaphores = fence.has_value() ? &semaphore : nullptr};
      return device.submit(std::span{&info, 1}, VK_NULL_HANDLE).has_value();
    }

    auto buffer() const noexcept -> VkCommandBuffer { return buffer_; }

   private:
    Commands(VulkanCommandPoolHandle pool, VkCommandBuffer buffer) noexcept
        : pool_(std::move(pool)), buffer_(buffer) {}

    VulkanCommandPoolHandle pool_;
    VkCommandBuffer buffer_;
  };

  static auto factory(headless::HeadlessProviderFactory& headless,
                      VulkanGpu& gpu) -> DefaultProviderFactory {
    headless.surface().resize(extent);
    headless.surface().set_scale(SurfaceScale::unit());
    return DefaultProviderFactory{
        Providers{.threading = headless.threading(),
                  .clock = headless.clock(),
                  .diagnostics = headless.diagnostics(),
                  .assets = headless.assets(),
                  .lifecycle = headless.lifecycle(),
                  .system_info = headless.system_info(),
                  .haptics = headless.haptics(),
                  .audio = headless.audio(),
                  .clipboard = headless.clipboard(),
                  .browser = headless.browser(),
                  .fonts = headless.fonts(),
                  .surface = headless.surface(),
                  .input = headless.input(),
                  .gpu = gpu,
                  .text_input = headless.text_input()}};
  }

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

  static auto quad(GpuFrame& frame, TextureId texture) -> bool {
    const std::uint32_t white = Vertex::rgba(255, 255, 255, 255);
    const auto right = static_cast<float>(width);
    const auto bottom = static_cast<float>(height);
    const std::array<Vertex, 4> vertices{
        Vertex{.position = {0.0F, 0.0F}, .uv = {0.0F, 0.0F}, .colour = white},
        Vertex{.position = {right, 0.0F}, .uv = {1.0F, 0.0F}, .colour = white},
        Vertex{
            .position = {right, bottom}, .uv = {1.0F, 1.0F}, .colour = white},
        Vertex{
            .position = {0.0F, bottom}, .uv = {0.0F, 1.0F}, .colour = white}};
    const std::array<Index, 6> indices{0, 1, 2, 0, 2, 3};
    const std::array commands{DrawCommand{
        .texture = texture,
        .clip = ClipRect{.x = 0, .y = 0, .width = width, .height = height},
        .index_offset = 0,
        .index_count = 6,
        .vertex_offset = 0}};
    auto geometry = frame.allocate_geometry(vertices.size(), indices.size());
    if (!geometry.has_value()) {
      return false;
    }
    std::ranges::copy(vertices, geometry->vertices.begin());
    std::ranges::copy(indices, geometry->indices.begin());
    if (!frame.draw(DrawData{.geometry = *geometry, .commands = commands})
             .has_value()) {
      return false;
    }
    frame.plan(FramePlan{.predicted_present = std::chrono::nanoseconds{0},
                         .replay = false,
                         .exclusive = false,
                         .extent = extent,
                         .swap_interval = 1,
                         .capture = false});
    return true;
  }

  static auto publish(FrameSource& frames, TextureId texture) -> bool {
    auto frame = frames.open();
    if (!frame.has_value() || !quad(*frame, texture)) {
      return false;
    }
    auto sealed = frame->seal();
    return sealed.has_value() && sealed->submit().has_value();
  }

  static auto record(const VulkanDevice& device, VulkanGpu::Overlay& overlay,
                     Commands& commands, VulkanBuffer& readback,
                     VulkanGpu::FrameTarget target)
      -> std::optional<std::array<std::uint8_t, 4>> {
    if (!commands.begin()) {
      return std::nullopt;
    }
    commands.transition(target.image, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    target.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    const auto fence = overlay.record(commands.buffer(), target);
    if (!fence.has_value() || !fence->has_value()) {
      static_cast<void>(commands.submit(device, std::nullopt));
      device.wait_idle();
      return std::nullopt;
    }
    commands.transition(target.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_ACCESS_TRANSFER_READ_BIT);
    commands.read_back(target.image, readback.handle());
    if (!commands.submit(device, *fence)) {
      return std::nullopt;
    }
    device.wait_idle();
    readback.invalidate();
    const std::span<const std::byte> pixels = readback.bytes();
    const std::size_t at = ((8 * width) + 8) * 4;
    return std::array<std::uint8_t, 4>{
        std::to_integer<std::uint8_t>(pixels[at]),
        std::to_integer<std::uint8_t>(pixels[at + 1]),
        std::to_integer<std::uint8_t>(pixels[at + 2]),
        std::to_integer<std::uint8_t>(pixels[at + 3])};
  }

  static auto view_of(const VulkanDevice& device,
                      const VulkanGpu::FrameTarget& target)
      -> std::optional<VulkanImageViewHandle> {
    const VkImageViewCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = target.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = target.format,
        .components = VkComponentMapping{.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                         .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                         .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                         .a = VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange =
            VkImageSubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                    .baseMipLevel = 0,
                                    .levelCount = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1}};
    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(device.handle(), &info, nullptr, &view) !=
        VK_SUCCESS) {
      return std::nullopt;
    }
    return VulkanImageViewHandle{{.device = device.handle(), .object = view}};
  }

  static void frames(headless::HeadlessProviderFactory& headless,
                     runtime::Application& application, int count) {
    for (int index = 0; index < count; ++index) {
      ASSERT_TRUE(
          headless.clock().advance(std::chrono::milliseconds{16}).has_value());
      ASSERT_TRUE(application.frame().has_value());
    }
  }
};

TEST_F(VulkanOverlayTest, TheOverlayRecordsIntoTheAppCommandBufferEachFrame) {
  auto gpu = offscreen({.size = extent, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  headless::HeadlessProviderFactory headless;
  DefaultProviderFactory providers = factory(headless, *gpu);
  auto application =
      runtime::Application::create(providers, components::VStack());
  ASSERT_TRUE(application.has_value());
  frames(headless, *application, 2);
  auto taken = gpu->take_over();
  ASSERT_TRUE(taken.has_value());
  auto overlay = taken->overlay();
  if (!overlay.has_value() &&
      overlay.error().code() == ProviderErrorCode::unsupported_capability) {
    GTEST_SKIP() << "no timeline semaphores";
  }
  ASSERT_TRUE(overlay.has_value());
  composition::Observable<int> shade{0};
  runtime::Root dock{Dock{shade}, providers};
  auto embedded = application->embed(dock, overlay->frames(), extent);
  ASSERT_TRUE(embedded.has_value());
  frames(headless, *application, 2);
  auto commands = Commands::create(gpu->device());
  ASSERT_TRUE(commands.has_value());
  auto readback = VulkanBuffer::create(
      gpu->device(), static_cast<VkDeviceSize>(width) * height * 4,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT, MemoryUse::readback);
  ASSERT_TRUE(readback.has_value());
  std::uint64_t last = 0;
  for (int index = 0; index < 6; ++index) {
    shade.set(index);
    frames(headless, *application, 1);
    const auto lent = taken->acquire(VK_NULL_HANDLE);
    ASSERT_TRUE(lent.has_value());
    ASSERT_TRUE(commands->begin());
    VulkanGpu::FrameTarget target = lent->target;
    commands->transition(target.image, VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    target.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    const auto fence = overlay->record(commands->buffer(), target);
    ASSERT_TRUE(fence.has_value());
    ASSERT_TRUE(fence->has_value());
    EXPECT_GT((*fence)->value, last);
    last = (*fence)->value;
    commands->transition(target.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_ACCESS_TRANSFER_READ_BIT);
    commands->read_back(target.image, readback->handle());
    ASSERT_TRUE(commands->submit(gpu->device(), *fence));
    ASSERT_TRUE(taken->present(target.index).has_value());
    gpu->device().wait_idle();
    readback->invalidate();
    const std::span<const std::byte> pixels = readback->bytes();
    const std::size_t docked = ((4 * width) + 4) * 4;
    EXPECT_GT(std::to_integer<int>(pixels[docked]), 0xA0);
    EXPECT_LT(std::to_integer<int>(pixels[docked + 2]), 0x40);
  }
  overlay->release();
  EXPECT_FALSE(overlay->held());
  embedded->release();
  taken->release();
  frames(headless, *application, 2);
  EXPECT_EQ(gpu->takeover_counters().returned, 1U);
}

TEST_F(VulkanOverlayTest, AnEmptyOverlayLeavesNothingToSignal) {
  auto gpu = offscreen({.size = extent, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  headless::HeadlessProviderFactory headless;
  DefaultProviderFactory providers = factory(headless, *gpu);
  auto application =
      runtime::Application::create(providers, components::VStack());
  ASSERT_TRUE(application.has_value());
  auto taken = gpu->take_over();
  ASSERT_TRUE(taken.has_value());
  auto overlay = taken->overlay();
  if (!overlay.has_value() &&
      overlay.error().code() == ProviderErrorCode::unsupported_capability) {
    GTEST_SKIP() << "no timeline semaphores";
  }
  ASSERT_TRUE(overlay.has_value());
  runtime::Root blank{components::VStack(), providers};
  auto embedded = application->embed(blank, overlay->frames(), extent);
  ASSERT_TRUE(embedded.has_value());
  frames(headless, *application, 3);
  auto commands = Commands::create(gpu->device());
  ASSERT_TRUE(commands.has_value());
  const auto lent = taken->acquire(VK_NULL_HANDLE);
  ASSERT_TRUE(lent.has_value());
  ASSERT_TRUE(commands->begin());
  const auto fence = overlay->record(commands->buffer(), lent->target);
  ASSERT_TRUE(fence.has_value());
  EXPECT_FALSE(fence->has_value());
  ASSERT_TRUE(commands->submit(gpu->device(), std::nullopt));
  ASSERT_TRUE(taken->present(lent->target.index).has_value());
  taken->release();
  EXPECT_FALSE(overlay->held());
  const auto refused = overlay->record(commands->buffer(), lent->target);
  ASSERT_FALSE(refused.has_value());
  EXPECT_EQ(refused.error().detail(), ProviderErrorDetail::not_taken_over);
}

TEST_F(VulkanOverlayTest, ATextureMadeOutsideAnOverlayFrameIsUploadedFirst) {
  auto gpu = offscreen({.size = extent, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  auto taken = gpu->take_over();
  ASSERT_TRUE(taken.has_value());
  auto overlay = taken->overlay();
  if (!overlay.has_value() &&
      overlay.error().code() == ProviderErrorCode::unsupported_capability) {
    GTEST_SKIP() << "no timeline semaphores";
  }
  ASSERT_TRUE(overlay.has_value());
  auto texture = solid(*gpu, {255, 0, 0, 255});
  ASSERT_TRUE(texture.has_value());
  FrameSource frames = overlay->frames();
  ASSERT_TRUE(publish(frames, texture->id()));
  auto commands = Commands::create(gpu->device());
  ASSERT_TRUE(commands.has_value());
  auto readback = VulkanBuffer::create(
      gpu->device(), static_cast<VkDeviceSize>(width) * height * 4,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT, MemoryUse::readback);
  ASSERT_TRUE(readback.has_value());
  const auto lent = taken->acquire(VK_NULL_HANDLE);
  ASSERT_TRUE(lent.has_value());
  const auto pixel =
      record(gpu->device(), *overlay, *commands, *readback, lent->target);
  ASSERT_TRUE(pixel.has_value());
  EXPECT_GT((*pixel)[0], 0xA0);
  EXPECT_LT((*pixel)[1], 0x40);
  EXPECT_LT((*pixel)[2], 0x40);
  ASSERT_TRUE(taken->present(lent->target.index).has_value());
}

TEST_F(VulkanOverlayTest, ClosingTheOverlayWhileAFrameIsPaintedIsClean) {
  auto gpu = offscreen({.size = extent, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  auto taken = gpu->take_over();
  ASSERT_TRUE(taken.has_value());
  auto overlay = taken->overlay();
  if (!overlay.has_value() &&
      overlay.error().code() == ProviderErrorCode::unsupported_capability) {
    GTEST_SKIP() << "no timeline semaphores";
  }
  ASSERT_TRUE(overlay.has_value());
  FrameSource frames = overlay->frames();
  auto frame = frames.open();
  ASSERT_TRUE(frame.has_value());
  auto texture = solid(*gpu, {0, 255, 0, 255});
  ASSERT_TRUE(texture.has_value());
  ASSERT_TRUE(quad(*frame, texture->id()));
  auto sealed = frame->seal();
  ASSERT_TRUE(sealed.has_value());
  overlay->release();
  EXPECT_TRUE(frames.closed());
  const auto refused = sealed->submit();
  ASSERT_FALSE(refused.has_value());
  EXPECT_EQ(refused.error().detail(), ProviderErrorDetail::not_taken_over);
  taken->release();
  auto shown = gpu->begin_frame();
  ASSERT_TRUE(shown.has_value());
  shown->clear(Colour{.red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 1.0F});
  ASSERT_TRUE(quad(*shown, texture->id()));
  ASSERT_TRUE(shown->present().has_value());
  const auto image = gpu->capture();
  ASSERT_TRUE(image.has_value());
  const auto pixel = image->pixel(8, 8);
  ASSERT_TRUE(pixel.has_value());
  EXPECT_LT((*pixel)[0], 0x40);
  EXPECT_GT((*pixel)[1], 0xA0);
}

TEST_F(VulkanOverlayTest, TheFramebufferCacheFollowsTheTargetGeneration) {
  auto gpu = offscreen({.size = extent, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  auto taken = gpu->take_over();
  ASSERT_TRUE(taken.has_value());
  auto overlay = taken->overlay();
  if (!overlay.has_value() &&
      overlay.error().code() == ProviderErrorCode::unsupported_capability) {
    GTEST_SKIP() << "no timeline semaphores";
  }
  ASSERT_TRUE(overlay.has_value());
  auto texture = solid(*gpu, {255, 0, 0, 255});
  ASSERT_TRUE(texture.has_value());
  FrameSource frames = overlay->frames();
  ASSERT_TRUE(publish(frames, texture->id()));
  auto commands = Commands::create(gpu->device());
  ASSERT_TRUE(commands.has_value());
  auto readback = VulkanBuffer::create(
      gpu->device(), static_cast<VkDeviceSize>(width) * height * 4,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT, MemoryUse::readback);
  ASSERT_TRUE(readback.has_value());
  const auto lent = taken->acquire(VK_NULL_HANDLE);
  ASSERT_TRUE(lent.has_value());
  VulkanGpu::FrameTarget target = lent->target;
  const VulkanDevice& device = gpu->device();
  ASSERT_TRUE(record(device, *overlay, *commands, *readback, target));
  EXPECT_EQ(overlay->statistics().framebuffers, 1U);
  ++target.generation;
  ASSERT_TRUE(record(device, *overlay, *commands, *readback, target));
  EXPECT_EQ(overlay->statistics().framebuffers, 1U);
  EXPECT_EQ(overlay->statistics().retired_framebuffers, 1U);
  ASSERT_TRUE(record(device, *overlay, *commands, *readback, target));
  EXPECT_EQ(overlay->statistics().framebuffers, 1U);
  EXPECT_EQ(overlay->statistics().retired_framebuffers, 0U);
  std::vector<VulkanImageViewHandle> views;
  for (int index = 0; index < 10; ++index) {
    auto made = view_of(device, target);
    ASSERT_TRUE(made.has_value());
    VulkanGpu::FrameTarget other = target;
    other.view = made->get().object;
    views.push_back(std::move(*made));
    ASSERT_TRUE(record(device, *overlay, *commands, *readback, other));
    EXPECT_LE(overlay->statistics().framebuffers, 8U);
  }
  EXPECT_EQ(overlay->statistics().framebuffers, 8U);
  overlay->release();
  ASSERT_TRUE(taken->present(target.index).has_value());
}

}
