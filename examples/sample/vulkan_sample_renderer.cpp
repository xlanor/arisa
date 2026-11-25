#include <vulkan_sample_renderer.hpp>

#include <array>
#include <utility>

namespace arisa::examples {

using provider::vulkan::VulkanGpu;

VulkanSampleRenderer::VulkanSampleRenderer(
    Animation& animation, Painting screen, Painting card,
    VulkanTimeline timeline, std::vector<Frame> ring,
    provider::FrameHook backdrop, provider::GpuSurface surface) noexcept
    : animation_(animation),
      screen_(std::move(screen)),
      card_painting_(std::move(card)),
      timeline_(std::move(timeline)),
      ring_(std::move(ring)),
      backdrop_(std::move(backdrop)),
      card_(std::move(surface)) {}

VulkanSampleRenderer::~VulkanSampleRenderer() {
  card_.reset();
  backdrop_.reset();
  if (screen_.program != nullptr) {
    screen_.program->device().wait_idle();
  }
}

auto VulkanSampleRenderer::painting(const VulkanGpu& gpu, std::uint32_t slots)
    -> std::expected<Painting, std::string> {
  const std::array formats{VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM};
  auto program = VulkanPlasmaProgram::create(gpu.device(), formats);
  if (!program.has_value()) {
    return std::unexpected(program.error());
  }
  auto made = (*program)->uniforms(slots);
  if (!made.has_value()) {
    return std::unexpected(made.error());
  }
  return Painting{
      .program = std::move(*program),
      .slots = std::make_shared<const VulkanPlasmaProgram::Uniforms>(
          std::move(*made))};
}

auto VulkanSampleRenderer::frame(VulkanGpu& gpu,
                                 const VulkanPlasmaProgram& program)
    -> std::expected<Frame, std::string> {
  auto image = provider::vulkan::VulkanImage::create(
      gpu.device(), {.width = picture_width,
                     .height = picture_height,
                     .format = picture_format,
                     .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                              VK_IMAGE_USAGE_SAMPLED_BIT,
                     .swizzle = {}});
  if (!image.has_value()) {
    return std::unexpected(std::string{"the picture got no memory"});
  }
  auto framebuffer = program.framebuffer(
      image->view(), picture_format,
      VkExtent2D{.width = picture_width, .height = picture_height});
  if (!framebuffer.has_value()) {
    return std::unexpected(framebuffer.error());
  }
  auto recorder = VulkanRecorder::create(gpu.device());
  if (!recorder.has_value()) {
    return std::unexpected(recorder.error());
  }
  auto texture = gpu.import_texture(
      image->image(),
      provider::ImportDescription{.width = picture_width,
                                  .height = picture_height,
                                  .format = provider::TextureFormat::rgba8});
  if (!texture.has_value()) {
    return std::unexpected(std::string{"the picture could not be imported"});
  }
  return Frame{.image = std::move(*image),
               .framebuffer = std::move(*framebuffer),
               .recorder = std::move(*recorder),
               .painted = false,
               .texture = std::move(*texture)};
}

auto VulkanSampleRenderer::create(VulkanGpu& gpu, Animation& animation)
    -> std::expected<VulkanSampleRenderer, std::string> {
  auto screen = painting(gpu, 1);
  if (!screen.has_value()) {
    return std::unexpected(screen.error());
  }
  auto card = painting(gpu, 1 + ring_size);
  if (!card.has_value()) {
    return std::unexpected(card.error());
  }
  auto timeline = VulkanTimeline::create(gpu.device());
  if (!timeline.has_value()) {
    return std::unexpected(timeline.error());
  }
  std::vector<Frame> ring;
  for (std::size_t index = 0; index < ring_size; ++index) {
    auto made = frame(gpu, *card->program);
    if (!made.has_value()) {
      return std::unexpected(made.error());
    }
    ring.push_back(std::move(*made));
  }
  provider::FrameHook backdrop = gpu.add_frame_hook(
      provider::FrameStage::before_ui,
      [painting = *screen, &animation](VkCommandBuffer commands,
                                       const VulkanGpu::FrameTarget& target,
                                       const provider::FrameInfo& info) {
        const auto pass = painting.program->target(target);
        if (pass.has_value()) {
          painting.program->draw(
              commands, *pass, *painting.slots, 0,
              animation.advance(Animation::Pattern::plasma, info));
        }
      },
      provider::FrameHookOptions{.drive = provider::FrameDrive::continuous});
  auto surface = gpu.add_surface(
      provider::SurfaceOptions{.mode = provider::SurfaceMode::texture,
                               .drive = provider::FrameDrive::continuous},
      [painting = *card, &animation](VkCommandBuffer commands,
                                     const VulkanGpu::FrameTarget& target,
                                     const provider::SurfaceFrame& frame) {
        const auto pass = painting.program->target(target);
        if (pass.has_value()) {
          painting.program->draw(
              commands, *pass, *painting.slots, 0,
              animation.advance(Animation::Pattern::ripples, frame.frame));
        }
      });
  if (!surface.has_value()) {
    return std::unexpected(std::string{"the card surface was refused"});
  }
  VulkanSampleRenderer made{animation,          std::move(*screen),
                            std::move(*card),   std::move(*timeline),
                            std::move(ring),    std::move(backdrop),
                            std::move(*surface)};
  made.shown_ = ring_size - 1;
  static_cast<void>(made.repaint_picture());
  return made;
}

auto VulkanSampleRenderer::repaint_picture()
    -> std::optional<provider::TextureId> {
  const std::size_t next = (shown_ + 1) % ring_.size();
  Frame& frame = ring_[next];
  if (frame.texture.in_use()) {
    return std::nullopt;
  }
  auto commands = frame.recorder.begin();
  if (!commands.has_value()) {
    return std::nullopt;
  }
  VulkanRecorder::to_attachment(*commands, frame.image.image(),
                                frame.painted
                                    ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                    : VK_IMAGE_LAYOUT_UNDEFINED);
  card_painting_.program->draw(
      *commands,
      VulkanPlasmaProgram::Pass::covering(
          frame.framebuffer.get().object, picture_format,
          VkExtent2D{.width = picture_width, .height = picture_height}),
      *card_painting_.slots, 1 + next,
      animation_.get().latest(Animation::Pattern::tunnel, picture_width,
                              picture_height));
  VulkanRecorder::to_sampled(*commands, frame.image.image());
  const provider::vulkan::ReadySemaphore ready = timeline_.next();
  if (!frame.recorder.submit(ready).has_value()) {
    return std::nullopt;
  }
  frame.painted = true;
  frame.texture.publish(ready);
  shown_ = next;
  return frame.texture.id();
}

}
