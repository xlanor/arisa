#include <deko3d_video.hpp>

#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <limits>
#include <utility>

namespace arisa::examples {

Deko3dVideo::Deko3dVideo(QueueHandle queue, Allocation uniforms,
                         Deko3dPlasma::Program program,
                         std::vector<Frame> ring) noexcept
    : queue_(std::move(queue)),
      uniforms_(std::move(uniforms)),
      program_(program),
      ring_(std::move(ring)) {}

Deko3dVideo::~Deko3dVideo() {
  for (Frame& frame : ring_) {
    if (frame.pending) {
      static_cast<void>(frame.done.wait());
    }
  }
}

auto Deko3dVideo::create(provider::horizon::Deko3dGpu& gpu,
                         const Deko3dPlasma& plasma)
    -> std::expected<Deko3dVideo, std::string> {
  using provider::horizon::Deko3dMemoryKind;
  const dk::Device device = gpu.device();
  QueueHandle queue{
      dk::QueueMaker{device}.setFlags(DkQueueFlags_Graphics).create()};
  auto uniforms =
      gpu.memory().allocate(Deko3dMemoryKind::data, Deko3dPlasma::uniform_size,
                            DK_UNIFORM_BUF_ALIGNMENT);
  if (!queue.valid() || !uniforms.has_value()) {
    return std::unexpected(std::string{"the video queue could not be made"});
  }
  Deko3dPlasma::Program program = plasma.program();
  program.uniforms = uniforms->gpu_address();
  dk::ImageLayout layout;
  dk::ImageLayoutMaker{device}
      .setFlags(DkImageFlags_UsageRender | DkImageFlags_Usage2DEngine)
      .setFormat(DkImageFormat_RGBA8_Unorm)
      .setDimensions(width, height)
      .initialize(layout);
  if (layout.getSize() > std::numeric_limits<std::uint32_t>::max()) {
    return std::unexpected(std::string{"the video frames are too large"});
  }
  std::vector<Frame> ring;
  ring.reserve(ring_size);
  for (std::size_t index = 0; index < ring_size; ++index) {
    auto memory = gpu.memory().allocate(
        Deko3dMemoryKind::image, static_cast<std::uint32_t>(layout.getSize()),
        layout.getAlignment());
    auto command_memory = gpu.memory().allocate(
        Deko3dMemoryKind::data, command_size, DK_CMDMEM_ALIGNMENT);
    CmdBufHandle commands{dk::CmdBufMaker{device}.create()};
    if (!memory.has_value() || !command_memory.has_value() ||
        !commands.valid()) {
      return std::unexpected(
          std::string{"the video frames could not get GPU memory"});
    }
    dkCmdBufAddMemory(commands.get(), command_memory->memblock(),
                      command_memory->offset(), command_memory->size());
    dk::Image image;
    image.initialize(layout, memory->memblock(), memory->offset());
    auto imported = gpu.import_texture(
        image,
        provider::ImportDescription{.width = width,
                                    .height = height,
                                    .format = provider::TextureFormat::rgba8},
        support::ScopedCallback{[held = std::move(*memory)]() noexcept {
          static_cast<void>(held);
        }});
    if (!imported.has_value()) {
      return std::unexpected(
          std::string{"a video frame could not be imported"});
    }
    ring.push_back(Frame{.image = image,
                         .command_memory = std::move(*command_memory),
                         .commands = std::move(commands),
                         .done = {},
                         .pending = false,
                         .texture = std::move(*imported)});
  }
  return Deko3dVideo{std::move(queue), std::move(*uniforms), program,
                     std::move(ring)};
}

auto Deko3dVideo::produce(StageControl& control, std::chrono::nanoseconds now)
    -> bool {
  Frame& frame = ring_[next_ % ring_.size()];
  if (frame.texture.in_use()) {
    if (next_ > 0) {
      ring_[(next_ - 1) % ring_.size()].texture.publish();
    }
    return false;
  }
  if (frame.pending) {
    static_cast<void>(frame.done.wait());
    frame.pending = false;
  }
  const StageControl::Uniforms uniforms = control.advance(provider::FrameInfo{
      .serial = provider::GpuSerial{0},
      .predicted_present = now,
      .extent =
          provider::SurfaceSize{.width = provider::SurfaceWidth{width},
                                .height = provider::SurfaceHeight{height}},
      .replay = false,
      .damage = std::nullopt});
  dkCmdBufClear(frame.commands.get());
  dk::CmdBuf commands{frame.commands.get()};
  const dk::ImageView view{frame.image};
  Deko3dPlasma::draw(commands, Deko3dPlasma::Target::whole(view, width, height),
                     program_, uniforms);
  commands.barrier(DkBarrier_Fragments, 0);
  dk::Queue queue{queue_.get()};
  queue.submitCommands(commands.finishList());
  queue.signalFence(frame.done, true);
  frame.pending = true;
  frame.texture.publish(frame.done);
  control.show(frame.texture.id());
  ++next_;
  return true;
}

}
