#include <deko3d_sample_renderer.hpp>
#include <plasma_shaders.hpp>

#include <arisa/support/scoped_callback.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <utility>

namespace arisa::examples {

using provider::horizon::Deko3dGpu;
using provider::horizon::Deko3dMemory;
using provider::horizon::Deko3dMemoryKind;

Deko3dSampleRenderer::Deko3dSampleRenderer(
    Animation& animation, Allocation code, Allocation uniforms, Program program,
    Queue queue, Allocation picture_uniforms, std::vector<Frame> ring,
    provider::FrameHook backdrop, provider::GpuSurface card) noexcept
    : animation_(animation),
      code_(std::move(code)),
      uniforms_(std::move(uniforms)),
      program_(program),
      queue_(std::move(queue)),
      picture_uniforms_(std::move(picture_uniforms)),
      ring_(std::move(ring)),
      backdrop_(std::move(backdrop)),
      card_(std::move(card)) {}

Deko3dSampleRenderer::~Deko3dSampleRenderer() {
  for (Frame& frame : ring_) {
    if (frame.pending) {
      static_cast<void>(frame.done.wait());
    }
  }
}

auto Deko3dSampleRenderer::frame(Deko3dGpu& gpu)
    -> std::expected<Frame, std::string> {
  const dk::Device device = gpu.device();
  dk::ImageLayout layout;
  dk::ImageLayoutMaker{device}
      .setFlags(DkImageFlags_UsageRender | DkImageFlags_Usage2DEngine)
      .setFormat(DkImageFormat_RGBA8_Unorm)
      .setDimensions(picture_width, picture_height)
      .initialize(layout);
  if (layout.getSize() > std::numeric_limits<std::uint32_t>::max()) {
    return std::unexpected(std::string{"the picture is too large"});
  }
  auto memory = gpu.memory().allocate(
      Deko3dMemoryKind::image, static_cast<std::uint32_t>(layout.getSize()),
      layout.getAlignment());
  auto command_memory = gpu.memory().allocate(
      Deko3dMemoryKind::data, command_size, DK_CMDMEM_ALIGNMENT);
  CmdBuf commands{dk::CmdBufMaker{device}.create()};
  if (!memory.has_value() || !command_memory.has_value() || !commands.valid()) {
    return std::unexpected(std::string{"the picture could not get GPU memory"});
  }
  dkCmdBufAddMemory(commands.get(), command_memory->memblock(),
                    command_memory->offset(), command_memory->size());
  dk::Image image;
  image.initialize(layout, memory->memblock(), memory->offset());
  auto texture = gpu.import_texture(
      image,
      provider::ImportDescription{.width = picture_width,
                                  .height = picture_height,
                                  .format = provider::TextureFormat::rgba8},
      support::ScopedCallback{
          [held = std::move(*memory)]() noexcept { static_cast<void>(held); }});
  if (!texture.has_value()) {
    return std::unexpected(std::string{"the picture could not be imported"});
  }
  return Frame{.image = image,
               .command_memory = std::move(*command_memory),
               .commands = std::move(commands),
               .done = {},
               .pending = false,
               .texture = std::move(*texture)};
}

auto Deko3dSampleRenderer::create(Deko3dGpu& gpu, Animation& animation)
    -> std::expected<Deko3dSampleRenderer, std::string> {
  const std::array<std::span<const std::uint8_t>, 2> sources{
      PlasmaShaders::vertex(), PlasmaShaders::fragment()};
  std::array<std::uint32_t, 2> offsets{};
  std::uint64_t total = 0;
  for (std::size_t index = 0; index < sources.size(); ++index) {
    total = Deko3dMemory::align_up(total, DK_SHADER_CODE_ALIGNMENT);
    offsets[index] = static_cast<std::uint32_t>(total);
    total += sources[index].size();
  }
  auto code = gpu.memory().allocate(Deko3dMemoryKind::code,
                                    static_cast<std::uint32_t>(total),
                                    DK_SHADER_CODE_ALIGNMENT);
  auto uniforms = gpu.memory().allocate(Deko3dMemoryKind::data, uniform_size,
                                        DK_UNIFORM_BUF_ALIGNMENT);
  if (!code.has_value() || !uniforms.has_value()) {
    return std::unexpected(std::string{"the shaders could not get GPU memory"});
  }
  Program program;
  const std::array<std::reference_wrapper<dk::Shader>, 2> shaders{
      program.vertex, program.fragment};
  for (std::size_t index = 0; index < sources.size(); ++index) {
    std::ranges::copy(std::as_bytes(sources[index]),
                      code->cpu_bytes().subspan(offsets[index]).begin());
    dk::ShaderMaker{code->memblock(), code->offset() + offsets[index]}
        .initialize(shaders[index].get());
    if (!shaders[index].get().isValid()) {
      return std::unexpected(std::string{"the shaders could not be loaded"});
    }
  }
  Queue queue{
      dk::QueueMaker{gpu.device()}.setFlags(DkQueueFlags_Graphics).create()};
  auto picture_uniforms = gpu.memory().allocate(
      Deko3dMemoryKind::data, uniform_size, DK_UNIFORM_BUF_ALIGNMENT);
  if (!queue.valid() || !picture_uniforms.has_value()) {
    return std::unexpected(std::string{"the picture queue could not be made"});
  }
  std::vector<Frame> ring;
  for (std::size_t index = 0; index < ring_size; ++index) {
    auto made = frame(gpu);
    if (!made.has_value()) {
      return std::unexpected(made.error());
    }
    ring.push_back(std::move(*made));
  }
  const DkGpuAddr address = uniforms->gpu_address();
  provider::FrameHook backdrop = gpu.add_frame_hook(
      provider::FrameStage::before_ui,
      [program, address, &animation](dk::CmdBuf& commands,
                                     const FrameTarget& target,
                                     const provider::FrameInfo& info) {
        draw(commands, whole(target.view, target.width, target.height), program,
             address, animation.advance(Animation::Pattern::plasma, info));
      },
      provider::FrameHookOptions{.drive = provider::FrameDrive::continuous});
  auto card = gpu.add_surface(
      provider::SurfaceOptions{.mode = provider::SurfaceMode::texture,
                               .drive = provider::FrameDrive::continuous},
      [program, address, &animation](dk::CmdBuf& commands,
                                     const FrameTarget& target,
                                     const provider::SurfaceFrame& frame) {
        draw(commands,
             Target{.view = target.view,
                    .viewport = frame.viewport,
                    .scissor = frame.scissor},
             program, address,
             animation.advance(Animation::Pattern::ripples, frame.frame));
      });
  if (!card.has_value()) {
    return std::unexpected(std::string{"the card surface was refused"});
  }
  Deko3dSampleRenderer made{
      animation,       std::move(*code),    std::move(*uniforms),
      program,         std::move(queue),    std::move(*picture_uniforms),
      std::move(ring), std::move(backdrop), std::move(*card)};
  made.shown_ = ring_size - 1;
  static_cast<void>(made.repaint_picture());
  return made;
}

auto Deko3dSampleRenderer::repaint_picture()
    -> std::optional<provider::TextureId> {
  const std::size_t next = (shown_ + 1) % ring_.size();
  Frame& frame = ring_[next];
  if (frame.texture.in_use()) {
    return std::nullopt;
  }
  if (frame.pending) {
    static_cast<void>(frame.done.wait());
    frame.pending = false;
  }
  dkCmdBufClear(frame.commands.get());
  dk::CmdBuf commands{frame.commands.get()};
  const dk::ImageView view{frame.image};
  draw(commands, whole(view, picture_width, picture_height), program_,
       picture_uniforms_.gpu_address(),
       animation_.get().latest(Animation::Pattern::tunnel, picture_width,
                               picture_height));
  commands.barrier(DkBarrier_Fragments, 0);
  dk::Queue queue{queue_.get()};
  queue.submitCommands(commands.finishList());
  queue.signalFence(frame.done, true);
  frame.pending = true;
  frame.texture.publish(frame.done);
  shown_ = next;
  return frame.texture.id();
}

auto Deko3dSampleRenderer::whole(const DkImageView& view, std::uint32_t width,
                                 std::uint32_t height) -> Target {
  return Target{
      .view = view,
      .viewport = provider::PassRegion{.x = 0.0F,
                                       .y = 0.0F,
                                       .width = static_cast<float>(width),
                                       .height = static_cast<float>(height)},
      .scissor =
          provider::ClipRect{.x = 0, .y = 0, .width = width, .height = height}};
}

void Deko3dSampleRenderer::draw(dk::CmdBuf& commands, const Target& target,
                                const Program& program, DkGpuAddr uniforms,
                                const Animation::Uniforms& values) {
  commands.bindRenderTargets(&target.view);
  commands.setViewports(0, DkViewport{.x = target.viewport.x,
                                      .y = target.viewport.y,
                                      .width = target.viewport.width,
                                      .height = target.viewport.height,
                                      .near = 0.0F,
                                      .far = 1.0F});
  commands.setScissors(0, DkScissor{.x = target.scissor.x,
                                    .y = target.scissor.y,
                                    .width = target.scissor.width,
                                    .height = target.scissor.height});
  commands.bindShaders(DkStageFlag_GraphicsMask,
                       {&program.vertex, &program.fragment});
  commands.bindRasterizerState(dk::RasterizerState{}.setCullMode(DkFace_None));
  commands.bindColorState(dk::ColorState{}.setBlendEnable(0, false));
  commands.bindColorWriteState(dk::ColorWriteState{});
  commands.bindDepthStencilState(
      dk::DepthStencilState{}.setDepthTestEnable(false).setDepthWriteEnable(
          false));
  commands.bindVtxAttribState(nullptr);
  commands.bindVtxBufferState(nullptr);
  commands.bindUniformBuffer(DkStage_Fragment, 0, uniforms, uniform_size);
  commands.pushConstants(uniforms, uniform_size, 0, sizeof(values), &values);
  commands.draw(DkPrimitive_Triangles, 3, 1, 0, 0);
}

}
