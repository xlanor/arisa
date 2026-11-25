#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/surface_view.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/conformance/gpu_suite.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/default_factory.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/gpu_timings.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/gpu/surface_frame.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/gpu/vertex.hpp>
#include <arisa/provider/gpu/yuv_conversion.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/providers.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/provider/threading/thread_role.hpp>
#include <arisa/provider/threading/thread_spec.hpp>
#include <arisa/provider/vulkan/device_object.hpp>
#include <arisa/provider/vulkan/ready_semaphore.hpp>
#include <arisa/provider/vulkan/vulkan_buffer.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>
#include <arisa/provider/vulkan/vulkan_gpu.hpp>
#include <arisa/provider/vulkan/vulkan_image.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/support/scoped_callback.hpp>
#include <arisa/theme/colour.hpp>

#include <animation.hpp>
#include <gpu_stage_screen.hpp>
#include <sample_model.hpp>
#include <sample_screen.hpp>
#include <stage_control.hpp>
#include <vulkan_nv12_video.hpp>
#include <vulkan_plasma.hpp>
#include <vulkan_recorder.hpp>
#include <vulkan_sample_renderer.hpp>
#include <vulkan_timeline.hpp>
#include <vulkan_video.hpp>

#include "vulkan_test.hpp"

#include <stb_image_write.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <flat_map>
#include <format>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <print>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace arisa::provider::vulkan {

using namespace std::chrono_literals;

class VulkanContentTest : public VulkanTest {
 protected:
  static constexpr SurfaceSize small{.width = SurfaceWidth{64},
                                     .height = SurfaceHeight{48}};
  static constexpr SurfaceSize stage_size{.width = SurfaceWidth{240},
                                          .height = SurfaceHeight{120}};
  static constexpr ClipRect video{.x = 88, .y = 36, .width = 64, .height = 48};
  static constexpr int settle_limit = 60;
  static constexpr std::chrono::seconds completion_wait{5};

  using Colours = std::array<float, 4>;

  class Painter {
   public:
    explicit Painter(const VulkanDevice& device) : device_(device) {
      const VkAttachmentDescription attachment{
          .flags = 0,
          .format = OffscreenTarget::image_format,
          .samples = VK_SAMPLE_COUNT_1_BIT,
          .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
          .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
          .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
          .initialLayout = VulkanGpu::hook_layout,
          .finalLayout = VulkanGpu::hook_layout};
      const VkAttachmentReference reference{
          .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
      const VkSubpassDescription subpass{
          .flags = 0,
          .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
          .inputAttachmentCount = 0,
          .pInputAttachments = nullptr,
          .colorAttachmentCount = 1,
          .pColorAttachments = &reference,
          .pResolveAttachments = nullptr,
          .pDepthStencilAttachment = nullptr,
          .preserveAttachmentCount = 0,
          .pPreserveAttachments = nullptr};
      const VkRenderPassCreateInfo info{
          .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .attachmentCount = 1,
          .pAttachments = &attachment,
          .subpassCount = 1,
          .pSubpasses = &subpass,
          .dependencyCount = 0,
          .pDependencies = nullptr};
      VkRenderPass made = VK_NULL_HANDLE;
      EXPECT_EQ(vkCreateRenderPass(device.handle(), &info, nullptr, &made),
                VK_SUCCESS);
      pass_ =
          VulkanRenderPassHandle{{.device = device.handle(), .object = made}};
    }

    Painter(const Painter&) = delete;
    auto operator=(const Painter&) -> Painter& = delete;
    Painter(Painter&&) = delete;
    auto operator=(Painter&&) -> Painter& = delete;

    ~Painter() { device_.get().wait_idle(); }

    void paint(VkCommandBuffer commands, const VulkanGpu::FrameTarget& target,
               VkRect2D area, Colours colour) {
      auto found = framebuffers_.find(target.view);
      if (found == framebuffers_.end()) {
        const VkFramebufferCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = pass_.get().object,
            .attachmentCount = 1,
            .pAttachments = &target.view,
            .width = target.extent.width,
            .height = target.extent.height,
            .layers = 1};
        VkFramebuffer made = VK_NULL_HANDLE;
        ASSERT_EQ(
            vkCreateFramebuffer(device_.get().handle(), &info, nullptr, &made),
            VK_SUCCESS);
        found = framebuffers_
                    .insert_or_assign(
                        target.view,
                        VulkanFramebufferHandle{
                            {.device = device_.get().handle(), .object = made}})
                    .first;
      }
      const VkRenderPassBeginInfo begin{
          .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
          .pNext = nullptr,
          .renderPass = pass_.get().object,
          .framebuffer = found->second.get().object,
          .renderArea =
              VkRect2D{.offset = {.x = 0, .y = 0}, .extent = target.extent},
          .clearValueCount = 0,
          .pClearValues = nullptr};
      vkCmdBeginRenderPass(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
      const VkClearAttachment attachment{
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .colorAttachment = 0,
          .clearValue = VkClearValue{
              .color = VkClearColorValue{
                  .float32 = {colour[0], colour[1], colour[2], colour[3]}}}};
      const VkClearRect rectangle{
          .rect = area, .baseArrayLayer = 0, .layerCount = 1};
      vkCmdClearAttachments(commands, 1, &attachment, 1, &rectangle);
      vkCmdEndRenderPass(commands);
    }

   private:
    std::reference_wrapper<const VulkanDevice> device_;
    VulkanRenderPassHandle pass_;
    std::flat_map<VkImageView, VulkanFramebufferHandle> framebuffers_;
  };

  struct PlaneBytes {
    VkImageAspectFlags aspect{VK_IMAGE_ASPECT_COLOR_BIT};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::byte> bytes;
  };

  static auto upload(const VulkanDevice& device,
                     examples::VulkanRecorder& recorder, VkImage image,
                     std::span<const PlaneBytes> planes, ReadySemaphore signal)
      -> bool {
    std::size_t total = 0;
    for (const PlaneBytes& plane : planes) {
      total += plane.bytes.size();
    }
    auto staging = VulkanBuffer::create(
        device, total, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryUse::upload);
    if (!staging.has_value()) {
      return false;
    }
    std::size_t at = 0;
    for (const PlaneBytes& plane : planes) {
      std::ranges::copy(plane.bytes, staging->bytes().begin() +
                                         static_cast<std::ptrdiff_t>(at));
      at += plane.bytes.size();
    }
    staging->flush(0, total);
    auto commands = recorder.begin();
    if (!commands.has_value()) {
      return false;
    }
    const auto barrier = [&](VkImageLayout from, VkImageLayout to,
                             VkPipelineStageFlags before, VkAccessFlags wrote,
                             VkPipelineStageFlags after, VkAccessFlags reads) {
      const VkImageMemoryBarrier described{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .pNext = nullptr,
          .srcAccessMask = wrote,
          .dstAccessMask = reads,
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
      vkCmdPipelineBarrier(*commands, before, after, 0, 0, nullptr, 0, nullptr,
                           1, &described);
    };
    barrier(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    at = 0;
    for (const PlaneBytes& plane : planes) {
      const VkBufferImageCopy copy{
          .bufferOffset = at,
          .bufferRowLength = 0,
          .bufferImageHeight = 0,
          .imageSubresource =
              VkImageSubresourceLayers{.aspectMask = plane.aspect,
                                       .mipLevel = 0,
                                       .baseArrayLayer = 0,
                                       .layerCount = 1},
          .imageOffset = VkOffset3D{.x = 0, .y = 0, .z = 0},
          .imageExtent = VkExtent3D{
              .width = plane.width, .height = plane.height, .depth = 1}};
      vkCmdCopyBufferToImage(*commands, staging->handle(), image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
      at += plane.bytes.size();
    }
    barrier(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VulkanGpu::import_layout,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
    if (!recorder.submit(signal).has_value()) {
      return false;
    }
    device.wait_idle();
    return true;
  }

  static auto app_image(const VulkanDevice& device, VkFormat format,
                        std::uint32_t width, std::uint32_t height)
      -> std::optional<VulkanImage> {
    if (!device.supports(format, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                     VK_FORMAT_FEATURE_TRANSFER_DST_BIT)) {
      return std::nullopt;
    }
    auto image = VulkanImage::create(
        device,
        VulkanImage::Description{.width = width,
                                 .height = height,
                                 .format = format,
                                 .usage = VK_IMAGE_USAGE_SAMPLED_BIT |
                                          VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                 .swizzle = {}});
    if (!image.has_value()) {
      return std::nullopt;
    }
    return std::move(*image);
  }

  static auto bytes(std::initializer_list<int> values)
      -> std::vector<std::byte> {
    std::vector<std::byte> made;
    made.reserve(values.size());
    for (const int value : values) {
      made.push_back(static_cast<std::byte>(value));
    }
    return made;
  }

  static auto solid(VulkanGpu& gpu, std::array<std::uint8_t, 4> rgba)
      -> std::optional<Texture> {
    auto texture = gpu.create_texture({.width = 1, .height = 1});
    if (!texture.has_value()) {
      return std::nullopt;
    }
    const std::array<std::byte, 4> texel{std::byte{rgba[0]}, std::byte{rgba[1]},
                                         std::byte{rgba[2]},
                                         std::byte{rgba[3]}};
    if (!gpu.update_texture(*texture, {.x = 0, .y = 0, .width = 1, .height = 1},
                            texel)
             .has_value()) {
      return std::nullopt;
    }
    return std::move(*texture);
  }

  static void show(VulkanGpu& gpu, TextureId texture, ClipRect area,
                   std::optional<Colour> clear = Colour{.alpha = 1.0F},
                   std::optional<ClipRect> damage = std::nullopt,
                   std::optional<FramePlan> plan = std::nullopt) {
    const std::uint32_t white = Vertex::rgba(255, 255, 255, 255);
    const auto left = static_cast<float>(area.x);
    const auto top = static_cast<float>(area.y);
    const auto right = static_cast<float>(area.x + area.width);
    const auto bottom = static_cast<float>(area.y + area.height);
    const std::array vertices{
        Vertex{.position = {left, top}, .uv = {0.0F, 0.0F}, .colour = white},
        Vertex{.position = {right, top}, .uv = {1.0F, 0.0F}, .colour = white},
        Vertex{
            .position = {right, bottom}, .uv = {1.0F, 1.0F}, .colour = white},
        Vertex{
            .position = {left, bottom}, .uv = {0.0F, 1.0F}, .colour = white}};
    const std::array<Index, 6> indices{0, 1, 2, 0, 2, 3};
    const std::array commands{
        DrawCommand{.texture = texture,
                    .clip = ClipRect{.x = 0,
                                     .y = 0,
                                     .width = small.width.value(),
                                     .height = small.height.value()},
                    .index_offset = 0,
                    .index_count = 6,
                    .vertex_offset = 0}};
    auto frame = gpu.begin_frame();
    ASSERT_TRUE(frame.has_value());
    if (plan.has_value()) {
      frame->plan(*plan);
    }
    if (clear.has_value()) {
      frame->clear(*clear);
    }
    if (damage.has_value()) {
      frame->damage(*damage);
    }
    auto geometry = frame->allocate_geometry(vertices.size(), indices.size());
    ASSERT_TRUE(geometry.has_value());
    std::ranges::copy(vertices, geometry->vertices.begin());
    std::ranges::copy(indices, geometry->indices.begin());
    ASSERT_TRUE(
        frame->draw(DrawData{.geometry = *geometry, .commands = commands})
            .has_value());
    ASSERT_TRUE(frame->present().has_value());
  }

  static void show_yuv(VulkanGpu& gpu, const VulkanGpu::Imported& imported) {
    ASSERT_TRUE(gpu.publications().has_value());
    const auto published = gpu.publications()->get().find(imported.id());
    ASSERT_TRUE(published.has_value());
    ASSERT_TRUE(published->planar());
    const ShapeVertex base{.colour = 0xFFFFFFFFU,
                           .kind = ShapeKind::yuv,
                           .effect = published->conversion.levels(),
                           .detail = published->conversion.weights()};
    std::array<ShapeVertex, 4> vertices{base, base, base, base};
    vertices[0].position = {0.0F, 0.0F};
    vertices[0].uv = {0.0F, 0.0F};
    vertices[1].position = {64.0F, 0.0F};
    vertices[1].uv = {1.0F, 0.0F};
    vertices[2].position = {64.0F, 48.0F};
    vertices[2].uv = {1.0F, 1.0F};
    vertices[3].position = {0.0F, 48.0F};
    vertices[3].uv = {0.0F, 1.0F};
    const std::array<Index, 6> indices{0, 1, 2, 0, 2, 3};
    const std::array commands{
        DrawCommand{.texture = imported.id(),
                    .clip = ClipRect{.x = 0, .y = 0, .width = 64, .height = 48},
                    .index_offset = 0,
                    .index_count = 6,
                    .vertex_offset = 0,
                    .detail = published->chroma}};
    auto frame = gpu.begin_frame();
    ASSERT_TRUE(frame.has_value());
    frame->clear(Colour{.alpha = 1.0F});
    auto geometry = frame->allocate_shapes(vertices.size(), indices.size());
    ASSERT_TRUE(geometry.has_value());
    std::ranges::copy(vertices, geometry->vertices.begin());
    std::ranges::copy(indices, geometry->indices.begin());
    ASSERT_TRUE(
        frame->draw(ShapeDrawData{.geometry = *geometry, .commands = commands})
            .has_value());
    ASSERT_TRUE(frame->present().has_value());
  }

  static auto finish(VulkanGpu& gpu) -> bool {
    const GpuSerial serial{gpu.statistics().frames_presented};
    const auto deadline = std::chrono::steady_clock::now() + completion_wait;
    while (!gpu.completed(serial) &&
           std::chrono::steady_clock::now() < deadline) {
      std::this_thread::yield();
    }
    return gpu.completed(serial);
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

  static auto near(std::array<std::uint8_t, 4> seen,
                   std::array<std::uint8_t, 4> wanted) -> bool {
    return conformance::GpuSuite::matches(seen, wanted);
  }

  static auto contains(ClipRect outer, ClipRect inner) -> bool {
    return outer.x <= inner.x && outer.y <= inner.y &&
           outer.x + outer.width >= inner.x + inner.width &&
           outer.y + outer.height >= inner.y + inner.height;
  }

  class Mover final : public composition::Component {
   public:
    explicit Mover(const composition::Observable<bool>& moved)
        : moved_(moved) {}

    auto operator==(const Mover& other) const -> bool {
      return &moved_.get() == &other.moved_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const bool moved = context.read(moved_.get());
      return components::ZStack(components::VStack()
                                    .frame({.width = 16.0F, .height = 16.0F})
                                    .background(theme::Colour::hex(0x2050C0))
                                    .offset(moved ? 60.0F : -60.0F, 0.0F));
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> moved_;
  };

  class Screen final : public composition::Component {
   public:
    Screen(TextureId texture, const composition::Observable<bool>& moved)
        : texture_(texture), moved_(moved) {}

    auto operator==(const Screen& other) const -> bool {
      return texture_ == other.texture_ && &moved_.get() == &other.moved_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const bool moved = context.read(moved_.get());
      return components::ZStack(
          components::Image::External(texture_).frame(
              {.width = static_cast<float>(video.width),
               .height = static_cast<float>(video.height)}),
          components::VStack()
              .frame({.width = 96.0F, .height = 24.0F})
              .glass_effect(),
          components::VStack()
              .frame({.width = 16.0F, .height = 16.0F})
              .background(theme::Colour::hex(0x2050C0))
              .offset(moved ? -60.0F : -100.0F, -40.0F));
    }

   private:
    TextureId texture_;
    std::reference_wrapper<const composition::Observable<bool>> moved_;
  };

  class Stage {
   public:
    explicit Stage(VulkanGpu gpu) : gpu_(std::move(gpu)) {
      headless_.surface().resize(stage_size);
      headless_.surface().set_scale(SurfaceScale::unit());
      factory_.emplace(Providers{.threading = headless_.threading(),
                                 .clock = headless_.clock(),
                                 .diagnostics = headless_.diagnostics(),
                                 .assets = headless_.assets(),
                                 .lifecycle = headless_.lifecycle(),
                                 .system_info = headless_.system_info(),
                                 .haptics = headless_.haptics(),
                                 .audio = headless_.audio(),
                                 .clipboard = headless_.clipboard(),
                                 .browser = headless_.browser(),
                                 .fonts = headless_.fonts(),
                                 .surface = headless_.surface(),
                                 .input = headless_.input(),
                                 .gpu = gpu_,
                                 .text_input = headless_.text_input()});
    }

    void start(composition::Element content, bool worker) {
      auto created = runtime::Application::create(
          *factory_, std::move(content),
          runtime::ApplicationOptions{.executors = std::nullopt,
                                      .fonts = {},
                                      .atlas = {},
                                      .interaction = {},
                                      .appearance = std::nullopt,
                                      .launch_colour = std::nullopt,
                                      .debug_glass_coverage = false,
                                      .debug_full_walk = false,
                                      .debug_separate_backdrops = false,
                                      .render_thread = worker});
      ASSERT_TRUE(created.has_value());
      application_.emplace(std::move(*created));
    }

    void frame() {
      ASSERT_TRUE(headless_.clock().advance(16ms).has_value());
      ASSERT_TRUE(application_->frame().has_value());
    }

    void frames(int count) {
      for (int index = 0; index < count; ++index) {
        frame();
      }
    }

    void settle() {
      for (int index = 0; index < settle_limit; ++index) {
        frame();
        if (!application_->last_frame().presented) {
          return;
        }
      }
      ADD_FAILURE() << "the application never went idle";
    }

    auto presents_over(int count) -> std::uint64_t {
      const std::uint64_t before = gpu_.statistics().frames_presented;
      frames(count);
      return gpu_.statistics().frames_presented - before;
    }

    void replay(int count) {
      for (int index = 0; index < count; ++index) {
        ASSERT_TRUE(headless_.clock().advance(16ms).has_value());
        application_->step_animations();
      }
    }

    auto gpu() -> VulkanGpu& { return gpu_; }

    auto application() -> runtime::Application& { return *application_; }

    auto threading() -> ThreadingProvider& { return headless_.threading(); }

    void set_scale(SurfaceScale scale) { headless_.surface().set_scale(scale); }

    void resize(SurfaceSize size) { headless_.surface().resize(size); }

    void touch(PixelPoint point, TouchPhase phase) {
      InputFrame input;
      input.touches.push_back(
          Touch{.id = TouchId{3}, .position = point, .phase = phase});
      headless_.input().push(input);
      frame();
    }

   private:
    headless::HeadlessProviderFactory headless_;
    VulkanGpu gpu_;
    std::optional<DefaultProviderFactory> factory_;
    std::optional<runtime::Application> application_;
  };

  static auto bar_codes(std::array<float, 3> rgb) -> std::array<int, 3> {
    constexpr float red_weight = 0.2126F;
    constexpr float blue_weight = 0.0722F;
    constexpr float green_weight = 1.0F - red_weight - blue_weight;
    const float luma = (red_weight * rgb[0]) + (green_weight * rgb[1]) +
                       (blue_weight * rgb[2]);
    const float blue = (rgb[2] - luma) / (2.0F * (1.0F - blue_weight));
    const float red = (rgb[0] - luma) / (2.0F * (1.0F - red_weight));
    return {static_cast<int>(std::lround(16.0F + (219.0F * luma))),
            static_cast<int>(std::lround(128.0F + (224.0F * blue))),
            static_cast<int>(std::lround(128.0F + (224.0F * red)))};
  }

  static constexpr std::array<std::array<float, 3>, 8> bars{{
      {0.75F, 0.75F, 0.75F},
      {0.75F, 0.75F, 0.0F},
      {0.0F, 0.75F, 0.75F},
      {0.0F, 0.75F, 0.0F},
      {0.75F, 0.0F, 0.75F},
      {0.75F, 0.0F, 0.0F},
      {0.0F, 0.0F, 0.75F},
      {0.0F, 0.0F, 0.0F},
  }};
  static constexpr std::uint32_t bar_width = 8;
  static constexpr std::uint32_t bars_width = 64;
  static constexpr std::uint32_t bars_height = 4;

  static auto bar_planes(VkImageAspectFlags luma_aspect,
                         VkImageAspectFlags chroma_aspect)
      -> std::array<PlaneBytes, 2> {
    PlaneBytes luma{.aspect = luma_aspect,
                    .width = bars_width,
                    .height = bars_height,
                    .bytes = {}};
    PlaneBytes chroma{.aspect = chroma_aspect,
                      .width = bars_width / 2,
                      .height = bars_height / 2,
                      .bytes = {}};
    for (std::uint32_t row = 0; row < bars_height; ++row) {
      for (std::uint32_t column = 0; column < bars_width; ++column) {
        luma.bytes.push_back(
            static_cast<std::byte>(bar_codes(bars[column / bar_width])[0]));
      }
    }
    for (std::uint32_t row = 0; row < bars_height / 2; ++row) {
      for (std::uint32_t column = 0; column < bars_width / 2; ++column) {
        const std::array<int, 3> codes =
            bar_codes(bars[(column * 2) / bar_width]);
        chroma.bytes.push_back(static_cast<std::byte>(codes[1]));
        chroma.bytes.push_back(static_cast<std::byte>(codes[2]));
      }
    }
    return {std::move(luma), std::move(chroma)};
  }

  static auto bars_description() -> ImportDescription {
    return ImportDescription{.width = bars_width,
                             .height = bars_height,
                             .format = TextureFormat::nv12,
                             .filter = SamplerFilter::linear,
                             .premultiplied = false,
                             .colour_matrix = ColourMatrix::bt709,
                             .range = ColourRange::limited};
  }

  static auto worst_bar_error(VulkanGpu& gpu) -> double {
    const auto image = gpu.capture();
    EXPECT_TRUE(image.has_value());
    if (!image.has_value()) {
      return std::numeric_limits<double>::infinity();
    }
    double worst = 0.0;
    for (std::size_t bar = 0; bar < bars.size(); ++bar) {
      const auto seen = image->pixel(
          static_cast<std::uint32_t>((bar * bar_width) + (bar_width / 2)), 24);
      EXPECT_TRUE(seen.has_value());
      if (!seen.has_value()) {
        return std::numeric_limits<double>::infinity();
      }
      for (std::size_t channel = 0; channel < 3; ++channel) {
        const double wanted = static_cast<double>(bars[bar][channel]) * 255.0;
        worst = std::max(
            worst, std::abs(static_cast<double>((*seen)[channel]) - wanted));
      }
    }
    return worst;
  }
};

TEST_F(VulkanContentTest, HooksRunAroundTheUiWithTheirTargetAndFrameInfo) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  std::vector<std::string_view> order;
  std::optional<VulkanGpu::FrameTarget> seen;
  std::optional<FrameInfo> described;
  const FrameHook after = gpu->add_frame_hook(
      FrameStage::after_ui,
      [&order](VkCommandBuffer, const VulkanGpu::FrameTarget&,
               const FrameInfo&) { order.emplace_back("after"); });
  const FrameHook before = gpu->add_frame_hook(
      FrameStage::before_ui,
      [&](VkCommandBuffer commands, const VulkanGpu::FrameTarget& target,
          const FrameInfo& info) {
        order.emplace_back("before");
        EXPECT_NE(commands, VK_NULL_HANDLE);
        seen = target;
        described = info;
      });
  const FrameHook second = gpu->add_frame_hook(
      FrameStage::before_ui,
      [&order](VkCommandBuffer, const VulkanGpu::FrameTarget&,
               const FrameInfo&) { order.emplace_back("second"); });
  ASSERT_TRUE(gpu->frame_schedule().has_value());
  EXPECT_TRUE(gpu->frame_schedule()->get().installed());
  auto frame = gpu->begin_frame();
  ASSERT_TRUE(frame.has_value());
  frame->plan(FramePlan{.predicted_present = 5ms, .replay = true});
  frame->clear(Colour{});
  ASSERT_TRUE(frame->present().has_value());
  EXPECT_EQ(order,
            (std::vector<std::string_view>{"before", "second", "after"}));
  ASSERT_TRUE(seen.has_value());
  EXPECT_NE(seen->image, VK_NULL_HANDLE);
  EXPECT_NE(seen->view, VK_NULL_HANDLE);
  EXPECT_EQ(seen->format, OffscreenTarget::image_format);
  EXPECT_EQ(seen->extent.width, 64U);
  EXPECT_EQ(seen->extent.height, 48U);
  EXPECT_EQ(seen->layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  ASSERT_TRUE(described.has_value());
  EXPECT_EQ(described->serial, GpuSerial{1});
  EXPECT_EQ(described->serial, gpu->frame_schedule()->get().serial());
  EXPECT_EQ(described->extent.width.value(), 64U);
  EXPECT_TRUE(described->replay);
  EXPECT_EQ(described->predicted_present, 5ms);
  EXPECT_EQ(gpu->statistics().hooked_frames, 1U);
}

TEST_F(VulkanContentTest, HookPixelsSitUnderAndOverTheUiInOrder) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  auto red = solid(*gpu, {255, 0, 0, 255});
  ASSERT_TRUE(red.has_value());
  Painter painter{gpu->device()};
  const FrameHook under = gpu->add_frame_hook(
      FrameStage::before_ui,
      [&painter](VkCommandBuffer commands, const VulkanGpu::FrameTarget& target,
                 const FrameInfo&) {
        painter.paint(
            commands, target,
            VkRect2D{.offset = {.x = 0, .y = 0}, .extent = target.extent},
            {0.0F, 0.0F, 1.0F, 1.0F});
      });
  const FrameHook over = gpu->add_frame_hook(
      FrameStage::after_ui,
      [&painter](VkCommandBuffer commands, const VulkanGpu::FrameTarget& target,
                 const FrameInfo&) {
        painter.paint(commands, target,
                      VkRect2D{.offset = {.x = 20, .y = 36},
                               .extent = {.width = 20, .height = 12}},
                      {0.0F, 1.0F, 0.0F, 1.0F});
      });
  show(*gpu, red->id(), ClipRect{.x = 0, .y = 0, .width = 32, .height = 48},
       Colour{.red = 1.0F, .green = 1.0F, .blue = 1.0F, .alpha = 1.0F});
  EXPECT_TRUE(near(pixel(*gpu, 5, 5), {255, 0, 0, 255}));
  EXPECT_TRUE(near(pixel(*gpu, 50, 10), {0, 0, 255, 255}));
  EXPECT_TRUE(near(pixel(*gpu, 25, 42), {0, 255, 0, 255}));
  EXPECT_TRUE(near(pixel(*gpu, 35, 42), {0, 255, 0, 255}));
  EXPECT_TRUE(near(pixel(*gpu, 50, 42), {0, 0, 255, 255}));
}

TEST_F(VulkanContentTest, AHookThatRebindsStateLeavesTheUiPassIntact) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  auto red = solid(*gpu, {255, 0, 0, 255});
  ASSERT_TRUE(red.has_value());
  examples::StageControl control;
  auto plasma = examples::VulkanPlasma::create(*gpu, control);
  ASSERT_TRUE(plasma.has_value()) << plasma.error();
  const FrameHook skew = gpu->add_frame_hook(
      FrameStage::before_ui,
      [](VkCommandBuffer commands, const VulkanGpu::FrameTarget&,
         const FrameInfo&) {
        const VkViewport tiny{.x = 3.0F,
                              .y = 3.0F,
                              .width = 2.0F,
                              .height = 2.0F,
                              .minDepth = 0.0F,
                              .maxDepth = 1.0F};
        vkCmdSetViewport(commands, 0, 1, &tiny);
        const VkRect2D corner{.offset = {.x = 0, .y = 0},
                              .extent = {.width = 1, .height = 1}};
        vkCmdSetScissor(commands, 0, 1, &corner);
      });
  for (int index = 0; index < 3; ++index) {
    show(*gpu, red->id(), ClipRect{.x = 0, .y = 0, .width = 32, .height = 48},
         Colour{.alpha = 1.0F});
  }
  EXPECT_TRUE(near(pixel(*gpu, 5, 5), {255, 0, 0, 255}));
  EXPECT_TRUE(near(pixel(*gpu, 30, 40), {255, 0, 0, 255}));
  const auto beside = pixel(*gpu, 50, 24);
  EXPECT_GT(static_cast<int>(beside[0]) + beside[1] + beside[2], 60)
      << "the plasma hook drew nothing beside the UI";
  EXPECT_FALSE(near(beside, {255, 0, 0, 255}));
}

TEST_F(VulkanContentTest, HookedFramesRepaintInFullAndTimeTheHooks) {
  auto gpu = offscreen({.size = small, .images = 1, .preserved = true});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  auto red = solid(*gpu, {255, 0, 0, 255});
  ASSERT_TRUE(red.has_value());
  Painter painter{gpu->device()};
  std::atomic<int> calls{0};
  const FrameHook under = gpu->add_frame_hook(
      FrameStage::before_ui,
      [&painter, &calls](VkCommandBuffer commands,
                         const VulkanGpu::FrameTarget& target,
                         const FrameInfo&) {
        const bool first = calls.fetch_add(1) == 0;
        painter.paint(
            commands, target,
            VkRect2D{.offset = {.x = 0, .y = 0}, .extent = target.extent},
            first ? Colours{0.0F, 0.0F, 1.0F, 1.0F}
                  : Colours{1.0F, 1.0F, 0.0F, 1.0F});
      });
  show(*gpu, red->id(), ClipRect{.x = 0, .y = 0, .width = 32, .height = 48});
  show(*gpu, red->id(), ClipRect{.x = 0, .y = 0, .width = 32, .height = 48},
       Colour{.alpha = 1.0F},
       ClipRect{.x = 60, .y = 44, .width = 2, .height = 2});
  EXPECT_TRUE(near(pixel(*gpu, 5, 5), {255, 0, 0, 255}));
  EXPECT_TRUE(near(pixel(*gpu, 50, 10), {255, 255, 0, 255}));
  for (int index = 0; index < 3; ++index) {
    show(*gpu, red->id(), ClipRect{.x = 0, .y = 0, .width = 32, .height = 48});
    ASSERT_TRUE(finish(*gpu));
  }
  const GpuTimings timings = gpu->timings();
  std::println("hooked frame: interface {} ns, hooks {} ns",
               timings.interface.value_or(-1ns).count(),
               timings.hooks.value_or(-1ns).count());
  EXPECT_TRUE(timings.measured_on_gpu);
  ASSERT_TRUE(timings.hooks.has_value());
  EXPECT_GT(timings.hooks->count(), 0);
  EXPECT_EQ(gpu->statistics().hooked_frames, 5U);
}

TEST_F(VulkanContentTest, HooksRunOncePerPresentedFrameIncludingReplays) {
  auto made = offscreen({.size = stage_size, .images = 3});
  if (!made.has_value()) {
    GTEST_SKIP() << skip_reason(made.error());
  }
  Stage stage{std::move(*made)};
  std::atomic<int> calls{0};
  std::atomic<int> replays{0};
  const FrameHook hook = stage.gpu().add_frame_hook(
      FrameStage::before_ui, [&](VkCommandBuffer, const VulkanGpu::FrameTarget&,
                                 const FrameInfo& info) {
        calls.fetch_add(1);
        if (info.replay) {
          replays.fetch_add(1);
        }
      });
  composition::Observable<bool> moved{false};
  const std::uint64_t before = stage.gpu().statistics().frames_presented;
  stage.start(Mover{moved}, true);
  stage.frames(2);
  motion::with_animation(motion::Animation::linear(1.0),
                         [&moved] { moved.set(true); });
  stage.frames(2);
  const std::uint64_t replayed = stage.application().replayed_frames();
  stage.replay(20);
  stage.settle();
  const std::uint64_t presented =
      stage.gpu().statistics().frames_presented - before;
  std::println("presented {} frames, {} replayed, {} hook calls, {} on replays",
               presented, stage.application().replayed_frames() - replayed,
               calls.load(), replays.load());
  EXPECT_GT(stage.application().replayed_frames(), replayed);
  EXPECT_GT(replays.load(), 0);
  EXPECT_EQ(static_cast<std::uint64_t>(calls.load()), presented);
}

TEST_F(VulkanContentTest, ContinuousOnDemandAndRequestedFramesPresentAsAsked) {
  auto made = offscreen({.size = stage_size, .images = 3});
  if (!made.has_value()) {
    GTEST_SKIP() << skip_reason(made.error());
  }
  Stage stage{std::move(*made)};
  composition::Observable<bool> moved{false};
  stage.start(Mover{moved}, false);
  stage.settle();
  EXPECT_EQ(stage.presents_over(5), 0U);
  std::atomic<int> calls{0};
  const FrameHook hook = stage.gpu().add_frame_hook(
      FrameStage::after_ui,
      [&calls](VkCommandBuffer, const VulkanGpu::FrameTarget&,
               const FrameInfo&) { calls.fetch_add(1); },
      FrameHookOptions{.drive = FrameDrive::continuous,
                       .frame_rate = std::nullopt});
  EXPECT_EQ(stage.presents_over(10), 10U);
  EXPECT_EQ(calls.load(), 10);
  hook.set_drive(FrameDrive::on_demand);
  stage.settle();
  const int settled = calls.load();
  EXPECT_EQ(stage.presents_over(6), 0U);
  hook.request_frame();
  EXPECT_EQ(stage.presents_over(5), 1U);
  EXPECT_EQ(calls.load(), settled + 1);
}

TEST_F(VulkanContentTest, ImportedImagesSampleInPlaceAndReleaseAfterTheGpu) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  const VulkanDevice& device = gpu->device();
  std::println("timeline semaphores {}, multi-planar nv12 {}",
               device.capabilities().timeline_semaphores,
               device.capabilities().multi_planar_nv12);
  auto image = app_image(device, VK_FORMAT_R8G8B8A8_UNORM, 2, 1);
  ASSERT_TRUE(image.has_value());
  auto recorder = examples::VulkanRecorder::create(device);
  ASSERT_TRUE(recorder.has_value());
  auto timeline = examples::VulkanTimeline::create(device);
  ASSERT_TRUE(timeline.has_value()) << timeline.error();
  const ReadySemaphore ready = timeline->next();
  const std::array texels{
      PlaneBytes{.aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                 .width = 2,
                 .height = 1,
                 .bytes = bytes({255, 0, 0, 255, 0, 0, 255, 255})}};
  const bool staging = upload(device, *recorder, image->image(), texels, ready);
  ASSERT_TRUE(staging);
  const VulkanGpu::Statistics before = gpu->statistics();
  const std::uint64_t created = gpu->resources().textures_created;
  bool released = false;
  auto sharp = gpu->import_texture(
      image->image(),
      ImportDescription{.width = 2,
                        .height = 1,
                        .format = TextureFormat::rgba8,
                        .filter = SamplerFilter::nearest},
      support::ScopedCallback{[&released]() noexcept { released = true; }});
  ASSERT_TRUE(sharp.has_value());
  std::optional<VulkanGpu::Imported> nearest{std::move(*sharp)};
  auto soft = gpu->import_texture(image->image(),
                                  ImportDescription{.width = 2, .height = 1});
  ASSERT_TRUE(soft.has_value());
  std::optional<VulkanGpu::Imported> linear{std::move(*soft)};
  EXPECT_FALSE(gpu->import_texture(VkImage{VK_NULL_HANDLE},
                                   ImportDescription{.width = 2, .height = 1})
                   .has_value());
  EXPECT_FALSE(
      gpu->import_texture(
             image->image(),
             ImportDescription{
                 .width = 2, .height = 1, .format = TextureFormat::nv12})
          .has_value());
  nearest->publish(ready);
  show(*gpu, nearest->id(),
       ClipRect{.x = 0, .y = 0, .width = 64, .height = 48});
  EXPECT_TRUE(nearest->in_use());
  const VulkanGpu::Statistics shown = gpu->statistics();
  EXPECT_EQ(shown.imports - before.imports, 2U);
  EXPECT_EQ(shown.semaphore_waits - before.semaphore_waits, 1U);
  EXPECT_EQ(shown.texture_bytes_uploaded, before.texture_bytes_uploaded);
  EXPECT_EQ(gpu->resources().textures_created, created);
  EXPECT_TRUE(near(pixel(*gpu, 30, 24), {255, 0, 0, 255}));
  EXPECT_TRUE(near(pixel(*gpu, 33, 24), {0, 0, 255, 255}));
  nearest.reset();
  EXPECT_FALSE(released);
  ASSERT_TRUE(finish(*gpu));
  EXPECT_TRUE(released);
  linear->publish();
  show(*gpu, linear->id(), ClipRect{.x = 0, .y = 0, .width = 64, .height = 48});
  EXPECT_EQ(gpu->statistics().semaphore_waits - shown.semaphore_waits, 0U);
  const auto blended = pixel(*gpu, 30, 24);
  EXPECT_GT(blended[0], 64);
  EXPECT_GT(blended[2], 64);
  ASSERT_TRUE(finish(*gpu));
  EXPECT_FALSE(linear->in_use());
  linear.reset();
  ASSERT_TRUE(gpu->publications().has_value());
  EXPECT_EQ(gpu->publications()->get().live(), 0U);
}

TEST_F(VulkanContentTest, Bgra8Rgb565AndAlpha8ImportsSampleTheirChannels) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  const VulkanDevice& device = gpu->device();
  auto recorder = examples::VulkanRecorder::create(device);
  ASSERT_TRUE(recorder.has_value());
  struct Case {
    VkFormat format;
    TextureFormat described;
    std::vector<std::byte> texel;
    std::array<std::uint8_t, 4> wanted;
  };
  const std::array cases{Case{.format = VK_FORMAT_B8G8R8A8_UNORM,
                              .described = TextureFormat::bgra8,
                              .texel = bytes({255, 0, 0, 255}),
                              .wanted = {0, 0, 255, 255}},
                         Case{.format = VK_FORMAT_R5G6B5_UNORM_PACK16,
                              .described = TextureFormat::rgb565,
                              .texel = bytes({0x00, 0xF8}),
                              .wanted = {255, 0, 0, 255}},
                         Case{.format = VK_FORMAT_R8_UNORM,
                              .described = TextureFormat::alpha8,
                              .texel = bytes({255}),
                              .wanted = {255, 255, 255, 255}}};
  int checked = 0;
  for (const Case& each : cases) {
    auto image = app_image(device, each.format, 1, 1);
    if (!image.has_value()) {
      std::println("format {} cannot be sampled here",
                   static_cast<int>(each.format));
      continue;
    }
    const std::array texels{PlaneBytes{.aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                                       .width = 1,
                                       .height = 1,
                                       .bytes = each.texel}};
    const bool staging = upload(device, *recorder, image->image(), texels, {});
    ASSERT_TRUE(staging);
    auto imported = gpu->import_texture(
        image->image(), ImportDescription{.width = 1,
                                          .height = 1,
                                          .format = each.described,
                                          .filter = SamplerFilter::nearest});
    if (each.described == TextureFormat::rgb565 &&
        !device.capabilities().rgb565) {
      EXPECT_FALSE(imported.has_value());
      continue;
    }
    if (each.described == TextureFormat::alpha8 &&
        !device.capabilities().swizzle) {
      EXPECT_FALSE(imported.has_value());
      continue;
    }
    ASSERT_TRUE(imported.has_value());
    imported->publish();
    show(*gpu, imported->id(),
         ClipRect{.x = 0, .y = 0, .width = 64, .height = 48});
    EXPECT_TRUE(near(pixel(*gpu, 32, 24), each.wanted))
        << "format " << static_cast<int>(each.format);
    ASSERT_TRUE(finish(*gpu));
    ++checked;
  }
  EXPECT_GE(checked, 1);
}

TEST_F(VulkanContentTest, Nv12BarsThroughTwoImagesDecodeAndReleaseOnce) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  const VulkanDevice& device = gpu->device();
  auto luma = app_image(device, VK_FORMAT_R8_UNORM, bars_width, bars_height);
  auto chroma =
      app_image(device, VK_FORMAT_R8G8_UNORM, bars_width / 2, bars_height / 2);
  ASSERT_TRUE(luma.has_value());
  ASSERT_TRUE(chroma.has_value());
  auto recorder = examples::VulkanRecorder::create(device);
  ASSERT_TRUE(recorder.has_value());
  const std::array<PlaneBytes, 2> planes =
      bar_planes(VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
  const bool luma_staging =
      upload(device, *recorder, luma->image(), std::span{&planes[0], 1}, {});
  ASSERT_TRUE(luma_staging);
  const bool chroma_staging =
      upload(device, *recorder, chroma->image(), std::span{&planes[1], 1}, {});
  ASSERT_TRUE(chroma_staging);
  int released = 0;
  {
    const std::array images{luma->image(), chroma->image()};
    const std::array missing{luma->image(), VkImage{VK_NULL_HANDLE}};
    EXPECT_FALSE(gpu->import_texture(std::span<const VkImage>{missing},
                                     bars_description())
                     .has_value());
    EXPECT_FALSE(
        gpu->import_texture(luma->image(), bars_description()).has_value());
    auto imported = gpu->import_texture(
        std::span<const VkImage>{images}, bars_description(),
        support::ScopedCallback{[&released]() noexcept { ++released; }});
    ASSERT_TRUE(imported.has_value());
    imported->publish();
    show_yuv(*gpu, *imported);
    const double worst = worst_bar_error(*gpu);
    std::println("two-image NV12 bars: worst channel error {:.2f}/255", worst);
    EXPECT_LE(worst, 2.5);
    EXPECT_EQ(gpu->statistics().imports, 2U);
  }
  EXPECT_EQ(released, 0);
  ASSERT_TRUE(finish(*gpu));
  EXPECT_EQ(released, 1);
  for (int frame = 0; frame < 3; ++frame) {
    auto idle = gpu->begin_frame();
    ASSERT_TRUE(idle.has_value());
    ASSERT_TRUE(idle->present().has_value());
  }
  ASSERT_TRUE(finish(*gpu));
  EXPECT_EQ(released, 1);
}

TEST_F(VulkanContentTest, Nv12BarsThroughOneMultiPlanarImageDecode) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  const VulkanDevice& device = gpu->device();
  if (!device.capabilities().multi_planar_nv12) {
    GTEST_SKIP() << "the device cannot sample G8_B8R8_2PLANE_420_UNORM";
  }
  VkDevice const handle = device.handle();
  const VkImageCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
      .extent =
          VkExtent3D{.width = bars_width, .height = bars_height, .depth = 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
  VkImage raw = VK_NULL_HANDLE;
  if (vkCreateImage(handle, &info, nullptr, &raw) != VK_SUCCESS) {
    GTEST_SKIP() << "the multi-planar image could not be created";
  }
  const VulkanImageHandle image{{.device = handle, .object = raw}};
  VkMemoryRequirements requirements{};
  vkGetImageMemoryRequirements(handle, raw, &requirements);
  auto memory = device.allocate(requirements, MemoryUse::device_local,
                                MemoryTiling::optimal);
  ASSERT_TRUE(memory.has_value());
  ASSERT_EQ(vkBindImageMemory(handle, raw, memory->memory(), memory->offset()),
            VK_SUCCESS);
  auto recorder = examples::VulkanRecorder::create(device);
  ASSERT_TRUE(recorder.has_value());
  const std::array<PlaneBytes, 2> planes =
      bar_planes(VK_IMAGE_ASPECT_PLANE_0_BIT, VK_IMAGE_ASPECT_PLANE_1_BIT);
  const bool staging = upload(device, *recorder, raw, planes, {});
  ASSERT_TRUE(staging);
  int released = 0;
  {
    const std::array single{raw};
    auto imported = gpu->import_texture(
        std::span<const VkImage>{single}, bars_description(),
        support::ScopedCallback{[&released]() noexcept { ++released; }});
    ASSERT_TRUE(imported.has_value());
    imported->publish();
    show_yuv(*gpu, *imported);
    const double worst = worst_bar_error(*gpu);
    std::println("multi-planar NV12 bars: worst channel error {:.2f}/255",
                 worst);
    EXPECT_LE(worst, 2.5);
  }
  ASSERT_TRUE(finish(*gpu));
  EXPECT_EQ(released, 1);
  device.wait_idle();
}

TEST_F(VulkanContentTest,
       APublishIsAPaintOnlyFrameDamagingTheVideoAndItsGlass) {
  auto made = offscreen({.size = stage_size, .images = 1, .preserved = true});
  if (!made.has_value()) {
    GTEST_SKIP() << skip_reason(made.error());
  }
  Stage stage{std::move(*made)};
  const VulkanDevice& device = stage.gpu().device();
  auto image =
      app_image(device, VK_FORMAT_R8G8B8A8_UNORM, video.width, video.height);
  ASSERT_TRUE(image.has_value());
  auto recorder = examples::VulkanRecorder::create(device);
  auto timeline = examples::VulkanTimeline::create(device);
  ASSERT_TRUE(recorder.has_value());
  ASSERT_TRUE(timeline.has_value());
  const auto fill = [&](int level) {
    std::vector<std::byte> texels;
    for (std::uint32_t at = 0; at < video.width * video.height; ++at) {
      texels.push_back(static_cast<std::byte>(level));
      texels.push_back(static_cast<std::byte>(255 - level));
      texels.push_back(std::byte{64});
      texels.push_back(std::byte{255});
    }
    return std::array{PlaneBytes{.aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .width = video.width,
                                 .height = video.height,
                                 .bytes = std::move(texels)}};
  };
  const bool first = upload(device, *recorder, image->image(), fill(0), {});
  ASSERT_TRUE(first);
  auto imported = stage.gpu().import_texture(
      image->image(),
      ImportDescription{.width = video.width, .height = video.height});
  ASSERT_TRUE(imported.has_value());
  composition::Observable<bool> moved{false};
  stage.start(Screen{imported->id(), moved}, false);
  stage.settle();
  stage.frame();
  EXPECT_FALSE(stage.application().last_frame().presented);
  const VulkanGpu::Statistics before = stage.gpu().statistics();
  ASSERT_TRUE(finish(stage.gpu()));
  const auto previous = stage.gpu().capture();
  ASSERT_TRUE(previous.has_value());
  const ReadySemaphore ready = timeline->next();
  const bool second =
      upload(device, *recorder, image->image(), fill(200), ready);
  ASSERT_TRUE(second);
  imported->publish(ready);
  stage.frame();
  const runtime::FrameTimings& timings = stage.application().last_frame();
  const VulkanGpu::Statistics after = stage.gpu().statistics();
  EXPECT_TRUE(timings.presented);
  EXPECT_EQ(timings.evaluations, 0U);
  EXPECT_FALSE(timings.laid_out);
  EXPECT_EQ(timings.uploaded_vertices, 0U);
  EXPECT_EQ(after.frames_presented - before.frames_presented, 1U);
  EXPECT_EQ(after.texture_bytes_uploaded, before.texture_bytes_uploaded);
  EXPECT_EQ(after.imports, before.imports);
  EXPECT_EQ(after.semaphore_waits - before.semaphore_waits, 1U);
  const auto damage = stage.gpu().last_damage();
  ASSERT_TRUE(damage.has_value());
  std::println("publish damage {}x{} at {},{}", damage->width, damage->height,
               damage->x, damage->y);
  EXPECT_TRUE(contains(*damage, video));
  EXPECT_LT(damage->x, video.x);
  EXPECT_TRUE(contains(ClipRect{.x = 40, .y = 20, .width = 160, .height = 80},
                       *damage));
  ASSERT_TRUE(finish(stage.gpu()));
  const auto current = stage.gpu().capture();
  ASSERT_TRUE(current.has_value());
  EXPECT_NE(current->pixel(120, 75), previous->pixel(120, 75));
  EXPECT_EQ(current->pixel(10, 10), previous->pixel(10, 10));
  stage.frame();
  EXPECT_FALSE(stage.application().last_frame().presented);
}

TEST_F(VulkanContentTest,
       ImportAndPublishFromAnotherThreadWhileTheWorkerReplays) {
  auto made = offscreen({.size = stage_size, .images = 3});
  if (!made.has_value()) {
    GTEST_SKIP() << skip_reason(made.error());
  }
  Stage stage{std::move(*made)};
  const VulkanDevice& device = stage.gpu().device();
  auto image = app_image(device, VK_FORMAT_R8G8B8A8_UNORM, 4, 4);
  ASSERT_TRUE(image.has_value());
  auto recorder = examples::VulkanRecorder::create(device);
  ASSERT_TRUE(recorder.has_value());
  std::vector<std::byte> grey(64, std::byte{128});
  const std::array texels{PlaneBytes{.aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .width = 4,
                                     .height = 4,
                                     .bytes = std::move(grey)}};
  const bool staging = upload(device, *recorder, image->image(), texels, {});
  ASSERT_TRUE(staging);
  device.wait_idle();
  auto shown = stage.gpu().import_texture(
      image->image(), ImportDescription{.width = 4, .height = 4});
  ASSERT_TRUE(shown.has_value());
  composition::Observable<bool> moved{false};
  stage.start(Screen{shown->id(), moved}, true);
  stage.settle();
  motion::with_animation(motion::Animation::linear(1.0),
                         [&moved] { moved.set(true); });
  stage.frames(2);
  std::atomic<int> imported{0};
  std::atomic<int> failed{0};
  std::atomic<int> released{0};
  const std::uint64_t replayed = stage.application().replayed_frames();
  {
    auto decoder = stage.threading().start(
        ThreadSpec{.name = "test-decoder", .role = ThreadRole::app},
        [&](const std::stop_token& stop) {
          while (!stop.stop_requested()) {
            auto ring = stage.gpu().import_texture(
                image->image(), ImportDescription{.width = 4, .height = 4},
                support::ScopedCallback{
                    [&released]() noexcept { released.fetch_add(1); }});
            if (!ring.has_value()) {
              failed.fetch_add(1);
              continue;
            }
            ring->publish();
            static_cast<void>(ring->in_use());
            shown->publish();
            imported.fetch_add(1);
          }
        });
    ASSERT_TRUE(decoder.has_value());
    for (int index = 0; index < 30; ++index) {
      stage.replay(1);
      if (index % 5 == 0) {
        stage.frame();
      }
    }
  }
  stage.settle();
  ASSERT_TRUE(finish(stage.gpu()));
  std::println("thread imports {}, released {}, replayed {}", imported.load(),
               released.load(),
               stage.application().replayed_frames() - replayed);
  EXPECT_GT(imported.load(), 0);
  EXPECT_EQ(failed.load(), 0);
  EXPECT_EQ(released.load(), imported.load());
  EXPECT_GT(stage.application().replayed_frames(), replayed);
  ASSERT_TRUE(stage.gpu().publications().has_value());
  EXPECT_EQ(stage.gpu().publications()->get().live(), 1U);
}

TEST_F(VulkanContentTest, TheGpuStageRunsOnVulkanThroughEveryPage) {
  auto made = offscreen({.size = stage_size, .images = 3});
  if (!made.has_value()) {
    GTEST_SKIP() << skip_reason(made.error());
  }
  if (!made->device().capabilities().timeline_semaphores) {
    GTEST_SKIP() << "the gpu stage video needs timeline semaphores";
  }
  examples::StageControl control;
  Stage stage{std::move(*made)};
  auto plasma = examples::VulkanPlasma::create(stage.gpu(), control);
  ASSERT_TRUE(plasma.has_value()) << plasma.error();
  control.bind(plasma->hook());
  auto rgba = examples::VulkanVideo::create(stage.gpu(), *plasma);
  ASSERT_TRUE(rgba.has_value()) << rgba.error();
  auto nv12 = examples::VulkanNv12Video::create(stage.gpu(), *plasma, control);
  ASSERT_TRUE(nv12.has_value()) << nv12.error();
  stage.start(examples::GpuStageScreen{control}, true);
  const auto run = [&](int count) {
    for (int index = 0; index < count; ++index) {
      stage.frame();
      if (control.take_change() || control.producing()) {
        const auto now = std::chrono::nanoseconds{
            static_cast<std::int64_t>(index) * 16'000'000};
        static_cast<void>(control.video_format() ==
                                  examples::StageControl::VideoFormat::nv12
                              ? nv12->produce(control, now)
                              : rgba->produce(control, now));
      }
    }
  };
  run(6);
  const std::uint64_t hooked = stage.gpu().statistics().hooked_frames;
  EXPECT_GT(hooked, 0U);
  ASSERT_TRUE(finish(stage.gpu()));
  const auto plasma_frame = stage.gpu().capture();
  ASSERT_TRUE(plasma_frame.has_value());
  const auto corner = plasma_frame->pixel(230, 110);
  ASSERT_TRUE(corner.has_value());
  EXPECT_GT(static_cast<int>((*corner)[0]) + (*corner)[1] + (*corner)[2], 60);
  control.set_page(examples::StageControl::Page::video);
  run(8);
  const VulkanGpu::Statistics video_statistics = stage.gpu().statistics();
  EXPECT_GE(video_statistics.semaphore_waits, 1U);
  EXPECT_NE(control.frame().get(), TextureId{0});
  control.set_video_format(examples::StageControl::VideoFormat::nv12);
  run(8);
  EXPECT_GT(stage.gpu().statistics().semaphore_waits,
            video_statistics.semaphore_waits);
  control.set_full_screen(true);
  run(4);
  EXPECT_TRUE(stage.application().last_frame().exclusive ||
              !stage.application().last_frame().presented);
  control.set_full_screen(false);
  control.set_under(true);
  run(4);
  ASSERT_TRUE(finish(stage.gpu()));
  const auto shown = stage.gpu().capture();
  ASSERT_TRUE(shown.has_value());
  std::println(
      "gpu stage on Vulkan: {} hooked frames, {} semaphore waits, {} "
      "imports",
      stage.gpu().statistics().hooked_frames,
      stage.gpu().statistics().semaphore_waits,
      stage.gpu().statistics().imports);
}

TEST_F(VulkanContentTest, TheSampleRunsOnVulkanWithItsHookSurfaceAndPicture) {
  constexpr SurfaceSize sample_size{.width = SurfaceWidth{1280},
                                    .height = SurfaceHeight{720}};
  auto made = offscreen({.size = sample_size, .images = 3});
  if (!made.has_value()) {
    GTEST_SKIP() << skip_reason(made.error());
  }
  if (!made->device().capabilities().timeline_semaphores) {
    GTEST_SKIP() << "the sample picture needs timeline semaphores";
  }
  examples::Animation animation;
  Stage stage{std::move(*made)};
  stage.resize(sample_size);
  auto renderer =
      examples::VulkanSampleRenderer::create(stage.gpu(), animation);
  ASSERT_TRUE(renderer.has_value()) << renderer.error();
  examples::SampleModel model{animation, *renderer};
  stage.start(examples::SampleScreen{model}, true);
  stage.frames(12);
  ASSERT_TRUE(finish(stage.gpu()));
  const auto before = stage.gpu().capture();
  ASSERT_TRUE(before.has_value());
  stage.frames(12);
  ASSERT_TRUE(finish(stage.gpu()));
  const auto after = stage.gpu().capture();
  ASSERT_TRUE(after.has_value());
  const VulkanGpu::Statistics statistics = stage.gpu().statistics();
  EXPECT_GT(statistics.hooked_frames, 0U);
  EXPECT_GE(statistics.semaphore_waits, 2U);
  EXPECT_EQ(statistics.imports, examples::VulkanSampleRenderer::ring_size);
  EXPECT_NE(before->pixel(1100, 400), after->pixel(1100, 400));
  const std::string path =
      std::string{ARISA_TEST_OUTPUT} + "/sample_vulkan.png";
  EXPECT_NE(stbi_write_png(path.c_str(), static_cast<int>(after->width()),
                           static_cast<int>(after->height()), 4,
                           after->pixels().data(),
                           static_cast<int>(after->width()) * 4),
            0);
  std::println(
      "sample on Vulkan: {} hooked frames, {} semaphore waits, "
      "{} imports, written to {}",
      statistics.hooked_frames, statistics.semaphore_waits, statistics.imports,
      path);
}

class VulkanSurfaceTest : public VulkanContentTest {
 protected:
  enum class Scene : std::uint8_t { bare, glass, flat, backdrop, scrolling };

  static constexpr float view_width = 64.0F;
  static constexpr float view_height = 48.0F;
  static constexpr Colours painted{230.0F / 255.0F, 26.0F / 255.0F,
                                   51.0F / 255.0F, 1.0F};
  static constexpr std::array<std::uint8_t, 4> painted_pixel{230, 26, 51, 255};
  static constexpr std::uint32_t backdrop_rgb = 0x2050C0;
  static constexpr std::array<std::uint8_t, 4> backdrop_pixel{0x20, 0x50, 0xC0,
                                                              255};

  class Recorder {
   public:
    void record(const SurfaceFrame& frame) {
      const std::scoped_lock lock{mutex_};
      ++calls_;
      last_ = frame;
    }

    auto calls() const -> int {
      const std::scoped_lock lock{mutex_};
      return calls_;
    }

    auto last() const -> SurfaceFrame {
      const std::scoped_lock lock{mutex_};
      return last_;
    }

    void paint(bool enabled) {
      const std::scoped_lock lock{mutex_};
      paints_ = enabled;
    }

    auto paints() const -> bool {
      const std::scoped_lock lock{mutex_};
      return paints_;
    }

   private:
    mutable std::mutex mutex_;
    int calls_{0};
    SurfaceFrame last_{};
    bool paints_{true};
  };

  struct Controls {
    composition::Observable<bool> shown{true};
    composition::Observable<float> width{view_width};
    composition::Observable<bool> away{false};
  };

  class SurfaceScene final : public composition::Component {
   public:
    SurfaceScene(
        std::optional<std::reference_wrapper<const GpuSurface>> surface,
        Scene scene, const Controls& controls) noexcept
        : surface_(surface), scene_(scene), controls_(controls) {}

    auto operator==(const SurfaceScene& other) const -> bool {
      return scene_ == other.scene_ &&
             &controls_.get() == &other.controls_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const Controls& controls = controls_;
      const bool shown = context.read(controls.shown);
      const float wide = context.read(controls.width);
      const bool away = context.read(controls.away);
      const composition::FrameSpec size{.width = wide, .height = view_height};
      composition::Element shown_view =
          !surface_.has_value()
              ? composition::Element{components::VStack()
                                         .frame(size)
                                         .background(
                                             theme::Colour::hex(0xE61A33))}
          : shown
              ? composition::Element{components::SurfaceView(surface_->get())
                                         .frame(size)}
              : composition::Element{components::VStack().frame(size)};
      if (away) {
        shown_view = std::move(shown_view).offset(1000.0F, 0.0F);
      }
      switch (scene_) {
        case Scene::glass:
        case Scene::flat:
          return components::ZStack(
              std::move(shown_view),
              components::VStack()
                  .frame({.width = 96.0F, .height = 24.0F})
                  .glass_effect());
        case Scene::backdrop:
          return components::ZStack(
              components::VStack()
                  .frame({.width = 160.0F, .height = 100.0F})
                  .background(theme::Colour::hex(backdrop_rgb)),
              std::move(shown_view));
        case Scene::scrolling:
          return components::ScrollView(
                     components::VStack(
                         components::Spacer().frame({.height = 60.0F}),
                         std::move(shown_view),
                         components::Spacer().frame({.height = 400.0F}))
                         .frame({.max_width = composition::infinity}))
              .frame({.width = 200.0F, .height = 100.0F});
        case Scene::bare:
          break;
      }
      return components::ZStack(std::move(shown_view));
    }

   private:
    std::optional<std::reference_wrapper<const GpuSurface>> surface_;
    Scene scene_;
    std::reference_wrapper<const Controls> controls_;
  };

  class Rig {
   public:
    Rig(VulkanGpu gpu, Scene scene, SurfaceOptions options,
        SurfaceScale scale = SurfaceScale::unit(), bool worker = false)
        : stage_(std::move(gpu)), painter_(stage_.gpu().device()) {
      stage_.set_scale(scale);
      if (scene != Scene::flat) {
        auto made = stage_.gpu().add_surface(
            options, [this](VkCommandBuffer commands,
                            const VulkanGpu::FrameTarget& target,
                            const SurfaceFrame& frame) {
              if (recorder_.paints()) {
                const VkRect2D area{
                    .offset =
                        VkOffset2D{
                            .x = static_cast<std::int32_t>(frame.scissor.x),
                            .y = static_cast<std::int32_t>(frame.scissor.y)},
                    .extent = VkExtent2D{.width = frame.scissor.width,
                                         .height = frame.scissor.height}};
                painter_.paint(commands, target, area, painted);
              }
              recorder_.record(frame);
            });
        EXPECT_TRUE(made.has_value());
        surface_.emplace(std::move(*made));
      }
      std::optional<std::reference_wrapper<const GpuSurface>> shown;
      if (surface_.has_value()) {
        shown = std::cref(*surface_);
      }
      stage_.start(SurfaceScene{shown, scene, controls_}, worker);
    }

    Rig(const Rig&) = delete;
    auto operator=(const Rig&) -> Rig& = delete;
    Rig(Rig&&) = delete;
    auto operator=(Rig&&) -> Rig& = delete;
    ~Rig() { stage_.gpu().device().wait_idle(); }

    auto stage() -> Stage& { return stage_; }
    auto recorder() -> Recorder& { return recorder_; }
    auto controls() -> Controls& { return controls_; }
    auto surface() -> GpuSurface& { return *surface_; }

    auto colour(std::uint32_t x, std::uint32_t y)
        -> std::array<std::uint8_t, 4> {
      EXPECT_TRUE(finish(stage_.gpu()));
      return pixel(stage_.gpu(), x, y);
    }

    auto top_of_paint(std::uint32_t x) -> std::optional<std::uint32_t> {
      EXPECT_TRUE(finish(stage_.gpu()));
      const auto image = stage_.gpu().capture();
      if (!image.has_value()) {
        return std::nullopt;
      }
      for (std::uint32_t y = 0; y < image->height(); ++y) {
        const auto found = image->pixel(x, y);
        if (found.has_value() && near(*found, painted_pixel, 2)) {
          return y;
        }
      }
      return std::nullopt;
    }

   private:
    Stage stage_;
    Painter painter_;
    Recorder recorder_;
    Controls controls_;
    std::optional<GpuSurface> surface_;
  };

  static auto near(std::array<std::uint8_t, 4> left,
                   std::array<std::uint8_t, 4> right, int tolerance) -> bool {
    for (std::size_t channel = 0; channel < 3; ++channel) {
      if (std::abs(static_cast<int>(left[channel]) -
                   static_cast<int>(right[channel])) > tolerance) {
        return false;
      }
    }
    return true;
  }

  static constexpr ClipRect view{.x = 88, .y = 36, .width = 64, .height = 48};
  static constexpr SurfaceOptions on_demand{.mode = SurfaceMode::texture};
  static constexpr SurfaceOptions continuous{.mode = SurfaceMode::texture,
                                             .drive = FrameDrive::continuous};
  static constexpr SurfaceOptions underlay{.mode = SurfaceMode::underlay};
};

TEST_F(VulkanSurfaceTest,
       TextureSurfacesRunPerFrameOrOnRequestAsPaintOnlyFrames) {
  auto made = offscreen({.size = stage_size, .images = 1, .preserved = true});
  if (!made.has_value()) {
    GTEST_SKIP() << skip_reason(made.error());
  }
  Rig rig{std::move(*made), Scene::glass, continuous};
  rig.stage().frames(4);
  const std::uint64_t presented =
      rig.stage().gpu().statistics().frames_presented;
  const int calls = rig.recorder().calls();
  rig.stage().frames(10);
  EXPECT_EQ(rig.stage().gpu().statistics().frames_presented - presented, 10U);
  EXPECT_EQ(rig.recorder().calls() - calls, 10);
  EXPECT_EQ(rig.colour(view.x + 4, view.y + 4), painted_pixel);
  rig.surface().set_drive(FrameDrive::on_demand);
  rig.stage().settle();
  const int idle = rig.recorder().calls();
  const VulkanGpu::Statistics before = rig.stage().gpu().statistics();
  rig.surface().request_frame();
  rig.stage().frame();
  const runtime::FrameTimings& timings = rig.stage().application().last_frame();
  EXPECT_TRUE(timings.presented);
  EXPECT_EQ(timings.evaluations, 0U);
  EXPECT_EQ(timings.uploaded_vertices, 0U);
  EXPECT_EQ(rig.recorder().calls() - idle, 1);
  EXPECT_EQ(rig.stage().gpu().statistics().texture_bytes_uploaded,
            before.texture_bytes_uploaded);
  const auto damage = rig.stage().gpu().last_damage();
  ASSERT_TRUE(damage.has_value());
  std::println("surface request damage {}x{} at {},{}", damage->width,
               damage->height, damage->x, damage->y);
  EXPECT_TRUE(contains(*damage, view));
  EXPECT_LT(damage->x, view.x);
  EXPECT_TRUE(contains(ClipRect{.x = 60, .y = 30, .width = 120, .height = 60},
                       *damage));
  rig.stage().frame();
  EXPECT_FALSE(rig.stage().application().last_frame().presented);
}

TEST_F(VulkanSurfaceTest, TheTargetIsTheLaidOutPixelRectAtEveryScale) {
  for (const float factor : {1.0F, 1.25F}) {
    auto made = offscreen({.size = stage_size, .images = 2});
    if (!made.has_value()) {
      GTEST_SKIP() << skip_reason(made.error());
    }
    Rig rig{std::move(*made), Scene::bare, on_demand,
            SurfaceScale::create(factor).value_or(SurfaceScale::unit())};
    rig.stage().settle();
    const SurfaceFrame frame = rig.recorder().last();
    EXPECT_EQ(frame.frame.extent.width.value(),
              static_cast<std::uint32_t>(std::lround(view_width * factor)));
    EXPECT_EQ(frame.frame.extent.height.value(),
              static_cast<std::uint32_t>(std::lround(view_height * factor)));
    EXPECT_EQ(frame.scissor.width, frame.frame.extent.width.value());
  }
}

TEST_F(VulkanSurfaceTest, ResizesRemovalAndCullingFollowTheView) {
  auto made = offscreen({.size = stage_size, .images = 2});
  if (!made.has_value()) {
    GTEST_SKIP() << skip_reason(made.error());
  }
  Rig rig{std::move(*made), Scene::bare, continuous};
  rig.stage().frames(4);
  const std::size_t live = rig.stage().gpu().live_textures();
  rig.controls().width.set(96.0F);
  rig.stage().frames(2);
  EXPECT_EQ(rig.recorder().last().frame.extent.width.value(), 96U);
  ASSERT_TRUE(finish(rig.stage().gpu()));
  rig.stage().frames(3);
  EXPECT_EQ(rig.stage().gpu().live_textures(), live);
  EXPECT_EQ(rig.colour(view.x - 14, view.y + 4), painted_pixel);
  rig.controls().away.set(true);
  rig.stage().frame();
  const int culled = rig.recorder().calls();
  rig.stage().frames(4);
  EXPECT_EQ(rig.recorder().calls(), culled);
  rig.controls().away.set(false);
  rig.stage().frames(2);
  EXPECT_GT(rig.recorder().calls(), culled);
  rig.controls().shown.set(false);
  rig.stage().frame();
  const int removed = rig.recorder().calls();
  rig.stage().frames(4);
  EXPECT_EQ(rig.recorder().calls(), removed);
  ASSERT_TRUE(finish(rig.stage().gpu()));
  rig.stage().frames(2);
  EXPECT_EQ(rig.stage().gpu().live_textures(), live - 1);
}

TEST_F(VulkanSurfaceTest, ContentClipsInsideAScrollViewAndScrollsWithIt) {
  auto made = offscreen({.size = stage_size, .images = 1, .preserved = true});
  if (!made.has_value()) {
    GTEST_SKIP() << skip_reason(made.error());
  }
  Rig rig{std::move(*made), Scene::scrolling, on_demand};
  rig.stage().settle();
  EXPECT_EQ(rig.top_of_paint(120), std::optional<std::uint32_t>{70U});
  EXPECT_TRUE(near(rig.colour(120, 109), painted_pixel, 2));
  EXPECT_FALSE(near(rig.colour(120, 111), painted_pixel, 40));
  rig.stage().touch({.x = 120.0F, .y = 100.0F}, TouchPhase::began);
  for (int step = 1; step <= 6; ++step) {
    rig.stage().touch(
        {.x = 120.0F, .y = 100.0F - (static_cast<float>(step) * 8.0F)},
        TouchPhase::moved);
  }
  for (int hold = 0; hold < 6; ++hold) {
    rig.stage().touch({.x = 120.0F, .y = 52.0F}, TouchPhase::moved);
  }
  rig.stage().touch({.x = 120.0F, .y = 52.0F}, TouchPhase::ended);
  rig.stage().settle();
  EXPECT_EQ(rig.recorder().calls(), 1);
  const auto top = rig.top_of_paint(120);
  ASSERT_TRUE(top.has_value());
  EXPECT_LT(*top, 60U);
  EXPECT_GE(*top, 10U);
  EXPECT_FALSE(near(rig.colour(120, 9), painted_pixel, 40));
}

TEST_F(VulkanSurfaceTest, GlassOverTheSurfaceRefractsItsContent) {
  std::array<std::uint8_t, 4> refracted{};
  std::array<std::uint8_t, 4> reference{};
  for (const Scene scene : {Scene::glass, Scene::flat}) {
    auto made = offscreen({.size = stage_size, .images = 2});
    if (!made.has_value()) {
      GTEST_SKIP() << skip_reason(made.error());
    }
    Rig rig{std::move(*made), scene, on_demand};
    rig.stage().settle();
    (scene == Scene::glass ? refracted : reference) = rig.colour(120, 60);
  }
  EXPECT_TRUE(near(refracted, reference, 6))
      << static_cast<int>(refracted[0]) << "," << static_cast<int>(refracted[1])
      << "," << static_cast<int>(refracted[2]) << " vs "
      << static_cast<int>(reference[0]) << "," << static_cast<int>(reference[1])
      << "," << static_cast<int>(reference[2]);
  EXPECT_GT(static_cast<int>(refracted[0]),
            static_cast<int>(refracted[2]) + 60);
}

TEST_F(VulkanSurfaceTest, AnUnderlayDrawsPixelExactInsideAHoleInTheBackground) {
  auto made = offscreen({.size = stage_size, .images = 2});
  if (!made.has_value()) {
    GTEST_SKIP() << skip_reason(made.error());
  }
  Rig rig{std::move(*made), Scene::backdrop, underlay};
  rig.recorder().paint(false);
  rig.stage().settle();
  ASSERT_GT(rig.recorder().calls(), 0);
  const SurfaceFrame frame = rig.recorder().last();
  EXPECT_EQ(frame.mode, SurfaceMode::underlay);
  EXPECT_EQ(frame.viewport, (PassRegion{.x = static_cast<float>(view.x),
                                        .y = static_cast<float>(view.y),
                                        .width = view_width,
                                        .height = view_height}));
  EXPECT_EQ(frame.scissor, view);
  const std::array<std::uint8_t, 4> clear = rig.colour(4, 4);
  EXPECT_TRUE(near(rig.colour(view.x + 10, view.y + 10), clear, 1));
  EXPECT_EQ(rig.colour(view.x - 10, view.y + 10), backdrop_pixel);
  rig.recorder().paint(true);
  rig.surface().request_frame();
  rig.stage().frame();
  EXPECT_EQ(rig.colour(view.x, view.y), painted_pixel);
  EXPECT_EQ(rig.colour(view.x + view.width - 1, view.y + view.height - 1),
            painted_pixel);
  EXPECT_EQ(rig.colour(view.x - 1, view.y), backdrop_pixel);
  EXPECT_EQ(rig.colour(view.x + view.width, view.y + view.height),
            backdrop_pixel);
  EXPECT_GT(rig.stage().gpu().statistics().surface_calls, 0U);
}

TEST_F(VulkanSurfaceTest, TheStartupReportNamesTheDeviceOnce) {
  headless::HeadlessProviderFactory sink;
  auto made = VulkanGpu::create({.size = small, .images = 1},
                                std::ref(sink.diagnostics()));
  if (!made.has_value()) {
    GTEST_SKIP() << skip_reason(made.error());
  }
  const std::vector<Diagnostic> reported = sink.diagnostics().snapshot();
  const auto vulkan =
      std::ranges::count_if(reported, [](const Diagnostic& diagnostic) {
        return diagnostic.category == "arisa.vulkan";
      });
  EXPECT_EQ(vulkan, 1);
  const auto found =
      std::ranges::find_if(reported, [](const Diagnostic& diagnostic) {
        return diagnostic.category == "arisa.vulkan";
      });
  ASSERT_NE(found, reported.end());
  std::println("{}", found->message);
  EXPECT_NE(found->message.find(
                std::string_view{made->device().report().substr(0, 8)}),
            std::string::npos);
  EXPECT_NE(found->message.find("Vulkan "), std::string::npos);
  EXPECT_NE(found->message.find("driver"), std::string::npos);
  EXPECT_NE(found->message.find("timeline semaphores"), std::string::npos);
}

TEST_F(VulkanSurfaceTest, TheGpuStageSurfacePageRuns) {
  constexpr SurfaceSize page{.width = SurfaceWidth{1280},
                             .height = SurfaceHeight{720}};
  auto made = offscreen({.size = page, .images = 2});
  if (!made.has_value()) {
    GTEST_SKIP() << skip_reason(made.error());
  }
  examples::StageControl control;
  Stage stage{std::move(*made)};
  stage.resize(page);
  auto plasma = examples::VulkanPlasma::create(stage.gpu(), control);
  ASSERT_TRUE(plasma.has_value()) << plasma.error();
  control.bind(plasma->hook());
  control.bind_surfaces(plasma->texture_surface(), plasma->underlay_surface());
  control.set_page(examples::StageControl::Page::surface);
  stage.start(examples::GpuStageScreen{control}, true);
  stage.frames(12);
  const std::uint64_t calls = stage.gpu().statistics().surface_calls;
  EXPECT_GE(calls, 12U);
  ASSERT_TRUE(finish(stage.gpu()));
  const auto shown = stage.gpu().capture();
  ASSERT_TRUE(shown.has_value());
  if (const char* path = std::getenv("ARISA_SURFACE_CAPTURE")) {
    std::ofstream file{path, std::ios::binary};
    file << std::format("P6\n{} {}\n255\n", shown->width(), shown->height());
    const std::span<const std::byte> bytes = shown->pixels();
    for (std::size_t at = 0; at + 3 < bytes.size(); at += Image::channels) {
      file.write(reinterpret_cast<const char*>(bytes.data() + at), 3);
    }
  }
  control.set_surface_drive(FrameDrive::on_demand);
  stage.settle();
  const std::uint64_t idle = stage.gpu().statistics().surface_calls;
  stage.frame();
  EXPECT_EQ(stage.gpu().statistics().surface_calls, idle);
  control.request_surfaces();
  stage.frame();
  EXPECT_EQ(stage.gpu().statistics().surface_calls, idle + 2);
}
}
