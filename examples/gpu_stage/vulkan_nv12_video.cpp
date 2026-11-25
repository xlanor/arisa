#include <vulkan_nv12_video.hpp>

#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/surface/surface_size.hpp>

#include <array>
#include <span>
#include <utility>

namespace arisa::examples {

VulkanNv12Video::VulkanNv12Video(provider::vulkan::VulkanGpu& gpu,
                                 std::shared_ptr<VulkanPlasmaProgram> program,
                                 VulkanTimeline timeline,
                                 std::vector<Frame> ring) noexcept
    : gpu_(gpu),
      program_(std::move(program)),
      timeline_(std::move(timeline)),
      ring_(std::move(ring)) {}

VulkanNv12Video::~VulkanNv12Video() {
  if (program_ != nullptr) {
    program_->device().wait_idle();
  }
}

auto VulkanNv12Video::plane(const VulkanPlasmaProgram& program, VkFormat format,
                            const provider::TextureDescription& described)
    -> std::expected<Plane, std::string> {
  const VkExtent2D extent{.width = described.width, .height = described.height};
  auto image = provider::vulkan::VulkanImage::create(
      program.device(), provider::vulkan::VulkanImage::Description{
                            .width = extent.width,
                            .height = extent.height,
                            .format = format,
                            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                     VK_IMAGE_USAGE_SAMPLED_BIT,
                            .swizzle = {}});
  if (!image.has_value()) {
    return std::unexpected(std::string{"an NV12 plane got no memory"});
  }
  auto framebuffer = program.framebuffer(image->view(), format, extent);
  if (!framebuffer.has_value()) {
    return std::unexpected(framebuffer.error());
  }
  return Plane{.image = std::move(*image),
               .framebuffer = std::move(*framebuffer),
               .format = format,
               .extent = extent};
}

auto VulkanNv12Video::import(provider::vulkan::VulkanGpu& gpu,
                             const Frame& frame,
                             const provider::ImportDescription& description)
    -> std::optional<provider::vulkan::VulkanGpu::Imported> {
  const std::array planes{frame.luma.image.image(), frame.chroma.image.image()};
  auto imported =
      gpu.import_texture(std::span<const VkImage>{planes}, description);
  if (!imported.has_value()) {
    return std::nullopt;
  }
  return std::move(*imported);
}

auto VulkanNv12Video::create(provider::vulkan::VulkanGpu& gpu,
                             const VulkanPlasma& plasma,
                             const StageControl& control)
    -> std::expected<VulkanNv12Video, std::string> {
  const std::shared_ptr<VulkanPlasmaProgram>& program = plasma.program();
  auto timeline = VulkanTimeline::create(gpu.device());
  if (!timeline.has_value()) {
    return std::unexpected(timeline.error());
  }
  const provider::ImportDescription description =
      control.declared(width, height);
  std::vector<Frame> ring;
  ring.reserve(ring_size);
  for (std::size_t index = 0; index < ring_size; ++index) {
    auto luma = plane(*program, VK_FORMAT_R8_UNORM, description.plane(0));
    auto chroma = plane(*program, VK_FORMAT_R8G8_UNORM, description.plane(1));
    if (!luma.has_value() || !chroma.has_value()) {
      return std::unexpected(luma.has_value() ? chroma.error() : luma.error());
    }
    auto uniforms = program->uniforms(2);
    auto recorder = VulkanRecorder::create(gpu.device());
    if (!uniforms.has_value() || !recorder.has_value()) {
      return std::unexpected(
          std::string{"an NV12 frame could not be prepared"});
    }
    const std::array planes{luma->image.image(), chroma->image.image()};
    auto imported =
        gpu.import_texture(std::span<const VkImage>{planes}, description);
    if (!imported.has_value()) {
      return std::unexpected(
          std::string{"an NV12 frame could not be imported"});
    }
    ring.push_back(Frame{.luma = std::move(*luma),
                         .chroma = std::move(*chroma),
                         .uniforms = std::move(*uniforms),
                         .recorder = std::move(*recorder),
                         .drawn = false,
                         .description = description,
                         .texture = std::move(*imported)});
  }
  return VulkanNv12Video{gpu, program, std::move(*timeline), std::move(ring)};
}

void VulkanNv12Video::draw(VkCommandBuffer commands, const Frame& frame,
                           const Plane& plane, std::size_t slot,
                           const StageControl::Uniforms& values) const {
  VulkanRecorder::to_attachment(commands, plane.image.image(),
                                frame.drawn
                                    ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                    : VK_IMAGE_LAYOUT_UNDEFINED);
  program_->draw(
      commands,
      VulkanPlasmaProgram::Pass::covering(plane.framebuffer.get().object,
                                          plane.format, plane.extent),
      frame.uniforms, slot, values);
  VulkanRecorder::to_sampled(commands, plane.image.image());
}

auto VulkanNv12Video::produce(StageControl& control,
                              std::chrono::nanoseconds now) -> bool {
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
  const provider::ImportDescription wanted = control.declared(width, height);
  if (wanted != frame.description) {
    auto renewed = import(gpu_, frame, wanted);
    if (!renewed.has_value()) {
      return false;
    }
    frame.texture = std::move(*renewed);
    frame.description = wanted;
  }
  const StageControl::Uniforms uniforms = control.advance(provider::FrameInfo{
      .serial = provider::GpuSerial{0},
      .predicted_present = now,
      .extent =
          provider::SurfaceSize{.width = provider::SurfaceWidth{width},
                                .height = provider::SurfaceHeight{height}},
      .replay = false,
      .damage = std::nullopt});
  draw(*commands, frame, frame.luma, 0,
       uniforms.encoding(StageControl::Plane::luma, width));
  draw(*commands, frame, frame.chroma, 1,
       uniforms.encoding(StageControl::Plane::chroma, width));
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
