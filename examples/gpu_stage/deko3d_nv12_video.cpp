#include <deko3d_nv12_video.hpp>

#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <limits>
#include <span>
#include <utility>

namespace arisa::examples {

Deko3dNv12Video::Planes::Planes(Allocation luma_memory,
                                Allocation chroma_memory, dk::Image luma,
                                dk::Image chroma) noexcept
    : luma_memory_(std::move(luma_memory)),
      chroma_memory_(std::move(chroma_memory)),
      luma_(luma),
      chroma_(chroma) {}

auto Deko3dNv12Video::Planes::layout(dk::Device device, DkImageFormat format,
                                     std::uint32_t plane_width,
                                     std::uint32_t plane_height)
    -> dk::ImageLayout {
  dk::ImageLayout made;
  dk::ImageLayoutMaker{device}
      .setFlags(DkImageFlags_UsageRender | DkImageFlags_Usage2DEngine)
      .setFormat(format)
      .setDimensions(plane_width, plane_height)
      .initialize(made);
  return made;
}

auto Deko3dNv12Video::Planes::create(provider::horizon::Deko3dGpu& gpu)
    -> std::expected<std::shared_ptr<Planes>, std::string> {
  using provider::horizon::Deko3dMemoryKind;
  const provider::ImportDescription description{
      .width = width,
      .height = height,
      .format = provider::TextureFormat::nv12};
  const provider::TextureDescription chroma_plane = description.plane(1);
  const dk::ImageLayout luma_layout =
      layout(gpu.device(), DkImageFormat_R8_Unorm, width, height);
  const dk::ImageLayout chroma_layout =
      layout(gpu.device(), DkImageFormat_RG8_Unorm, chroma_plane.width,
             chroma_plane.height);
  if (luma_layout.getSize() > std::numeric_limits<std::uint32_t>::max() ||
      chroma_layout.getSize() > std::numeric_limits<std::uint32_t>::max()) {
    return std::unexpected(std::string{"the NV12 planes are too large"});
  }
  auto luma_memory =
      gpu.memory().allocate(Deko3dMemoryKind::image,
                            static_cast<std::uint32_t>(luma_layout.getSize()),
                            luma_layout.getAlignment());
  auto chroma_memory =
      gpu.memory().allocate(Deko3dMemoryKind::image,
                            static_cast<std::uint32_t>(chroma_layout.getSize()),
                            chroma_layout.getAlignment());
  if (!luma_memory.has_value() || !chroma_memory.has_value()) {
    return std::unexpected(
        std::string{"the NV12 planes could not get GPU memory"});
  }
  dk::Image luma;
  luma.initialize(luma_layout, luma_memory->memblock(), luma_memory->offset());
  dk::Image chroma;
  chroma.initialize(chroma_layout, chroma_memory->memblock(),
                    chroma_memory->offset());
  return std::make_shared<Planes>(std::move(*luma_memory),
                                  std::move(*chroma_memory), luma, chroma);
}

Deko3dNv12Video::Deko3dNv12Video(provider::horizon::Deko3dGpu& gpu,
                                 QueueHandle queue, Allocation uniforms,
                                 Deko3dPlasma::Program program,
                                 std::vector<Frame> ring) noexcept
    : gpu_(gpu),
      queue_(std::move(queue)),
      uniforms_(std::move(uniforms)),
      program_(program),
      ring_(std::move(ring)) {}

Deko3dNv12Video::~Deko3dNv12Video() {
  for (Frame& frame : ring_) {
    if (frame.pending) {
      static_cast<void>(frame.done.wait());
    }
  }
}

auto Deko3dNv12Video::import(provider::horizon::Deko3dGpu& gpu,
                             const std::shared_ptr<Planes>& planes,
                             const provider::ImportDescription& description)
    -> std::optional<provider::horizon::Deko3dGpu::Imported> {
  const std::array<const dk::Image*, 2> natives = planes->natives();
  auto imported = gpu.import_texture(
      std::span<const dk::Image* const>{natives}, description,
      support::ScopedCallback{
          [held = planes]() noexcept { static_cast<void>(held); }});
  if (!imported.has_value()) {
    return std::nullopt;
  }
  return std::move(*imported);
}

auto Deko3dNv12Video::create(provider::horizon::Deko3dGpu& gpu,
                             const Deko3dPlasma& plasma,
                             const StageControl& control)
    -> std::expected<Deko3dNv12Video, std::string> {
  using provider::horizon::Deko3dMemoryKind;
  const dk::Device device = gpu.device();
  QueueHandle queue{
      dk::QueueMaker{device}.setFlags(DkQueueFlags_Graphics).create()};
  auto uniforms =
      gpu.memory().allocate(Deko3dMemoryKind::data, Deko3dPlasma::uniform_size,
                            DK_UNIFORM_BUF_ALIGNMENT);
  if (!queue.valid() || !uniforms.has_value()) {
    return std::unexpected(std::string{"the NV12 queue could not be made"});
  }
  Deko3dPlasma::Program program = plasma.program();
  program.uniforms = uniforms->gpu_address();
  const provider::ImportDescription description =
      control.declared(width, height);
  std::vector<Frame> ring;
  ring.reserve(ring_size);
  for (std::size_t index = 0; index < ring_size; ++index) {
    auto planes = Planes::create(gpu);
    if (!planes.has_value()) {
      return std::unexpected(planes.error());
    }
    auto command_memory = gpu.memory().allocate(
        Deko3dMemoryKind::data, command_size, DK_CMDMEM_ALIGNMENT);
    CmdBufHandle commands{dk::CmdBufMaker{device}.create()};
    if (!command_memory.has_value() || !commands.valid()) {
      return std::unexpected(
          std::string{"the NV12 frames could not get GPU memory"});
    }
    dkCmdBufAddMemory(commands.get(), command_memory->memblock(),
                      command_memory->offset(), command_memory->size());
    auto imported = import(gpu, *planes, description);
    if (!imported.has_value()) {
      return std::unexpected(
          std::string{"an NV12 frame could not be imported"});
    }
    ring.push_back(Frame{.planes = std::move(*planes),
                         .description = description,
                         .command_memory = std::move(*command_memory),
                         .commands = std::move(commands),
                         .done = {},
                         .pending = false,
                         .texture = std::move(*imported)});
  }
  return Deko3dNv12Video{gpu, std::move(queue), std::move(*uniforms), program,
                         std::move(ring)};
}

auto Deko3dNv12Video::produce(StageControl& control,
                              std::chrono::nanoseconds now) -> bool {
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
  const provider::ImportDescription wanted = control.declared(width, height);
  if (wanted != frame.description) {
    auto renewed = import(gpu_, frame.planes, wanted);
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
  const provider::TextureDescription chroma = wanted.plane(1);
  dkCmdBufClear(frame.commands.get());
  dk::CmdBuf commands{frame.commands.get()};
  const dk::ImageView luma_view{frame.planes->luma()};
  const dk::ImageView chroma_view{frame.planes->chroma()};
  Deko3dPlasma::draw(
      commands, Deko3dPlasma::Target::whole(luma_view, width, height), program_,
      uniforms.encoding(StageControl::Plane::luma, width));
  Deko3dPlasma::draw(
      commands,
      Deko3dPlasma::Target::whole(chroma_view, chroma.width, chroma.height),
      program_, uniforms.encoding(StageControl::Plane::chroma, width));
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
