#include <vulkan_video.hpp>

#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <utility>

namespace arisa::examples {

VulkanVideo::VulkanVideo(std::shared_ptr<VulkanPlasmaProgram> program,
                         VulkanTimeline timeline,
                         std::vector<Frame> ring) noexcept
    : program_(std::move(program)),
      timeline_(std::move(timeline)),
      ring_(std::move(ring)) {}

VulkanVideo::~VulkanVideo() {
  if (program_ != nullptr) {
    program_->device().wait_idle();
  }
}

auto VulkanVideo::create(provider::vulkan::VulkanGpu& gpu,
                         const VulkanPlasma& plasma)
    -> std::expected<VulkanVideo, std::string> {
  const std::shared_ptr<VulkanPlasmaProgram>& program = plasma.program();
  const provider::vulkan::VulkanDevice& device = gpu.device();
  auto timeline = VulkanTimeline::create(device);
  if (!timeline.has_value()) {
    return std::unexpected(timeline.error());
  }
  std::vector<Frame> ring;
  ring.reserve(ring_size);
  for (std::size_t index = 0; index < ring_size; ++index) {
    auto image = provider::vulkan::VulkanImage::create(
        device, provider::vulkan::VulkanImage::Description{
                    .width = width,
                    .height = height,
                    .format = format,
                    .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                             VK_IMAGE_USAGE_SAMPLED_BIT,
                    .swizzle = {}});
    if (!image.has_value()) {
      return std::unexpected(std::string{"a video frame got no memory"});
    }
    auto framebuffer = program->framebuffer(
        image->view(), format, VkExtent2D{.width = width, .height = height});
    auto uniforms = program->uniforms(1);
    auto recorder = VulkanRecorder::create(device);
    if (!framebuffer.has_value() || !uniforms.has_value() ||
        !recorder.has_value()) {
      return std::unexpected(
          std::string{"a video frame could not be prepared"});
    }
    auto imported = gpu.import_texture(
        image->image(),
        provider::ImportDescription{.width = width,
                                    .height = height,
                                    .format = provider::TextureFormat::rgba8});
    if (!imported.has_value()) {
      return std::unexpected(
          std::string{"a video frame could not be imported"});
    }
    ring.push_back(Frame{.image = std::move(*image),
                         .framebuffer = std::move(*framebuffer),
                         .uniforms = std::move(*uniforms),
                         .recorder = std::move(*recorder),
                         .drawn = false,
                         .texture = std::move(*imported)});
  }
  return VulkanVideo{program, std::move(*timeline), std::move(ring)};
}

auto VulkanVideo::produce(StageControl& control, std::chrono::nanoseconds now)
    -> bool {
  Frame& frame = ring_[next_ % ring_.size()];
  if (frame.texture.in_use()) {
    if (next_ > 0) {
      ring_[(next_ - 1) % ring_.size()].texture.publish();
    }
    return false;
  }
  auto commands = frame.recorder.begin();
  if (!commands.has_value()) {
    return false;
  }
  const StageControl::Uniforms uniforms = control.advance(provider::FrameInfo{
      .serial = provider::GpuSerial{0},
      .predicted_present = now,
      .extent =
          provider::SurfaceSize{.width = provider::SurfaceWidth{width},
                                .height = provider::SurfaceHeight{height}},
      .replay = false,
      .damage = std::nullopt});
  VulkanRecorder::to_attachment(*commands, frame.image.image(),
                                frame.drawn
                                    ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                    : VK_IMAGE_LAYOUT_UNDEFINED);
  program_->draw(*commands,
                 VulkanPlasmaProgram::Pass::covering(
                     frame.framebuffer.get().object, format,
                     VkExtent2D{.width = width, .height = height}),
                 frame.uniforms, 0, uniforms);
  VulkanRecorder::to_sampled(*commands, frame.image.image());
  const provider::vulkan::ReadySemaphore ready = timeline_.next();
  if (!frame.recorder.submit(ready).has_value()) {
    return false;
  }
  frame.drawn = true;
  frame.texture.publish(ready);
  control.show(frame.texture.id());
  ++next_;
  return true;
}

}
