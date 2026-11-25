#include <deko3d_plasma.hpp>
#include <plasma_shaders.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <utility>

namespace arisa::examples {

Deko3dPlasma::Deko3dPlasma(Allocation code, Allocation uniforms,
                           Program program, provider::FrameHook hook,
                           provider::GpuSurface texture,
                           provider::GpuSurface underlay) noexcept
    : code_(std::move(code)),
      uniforms_(std::move(uniforms)),
      program_(program),
      hook_(std::move(hook)),
      texture_(std::move(texture)),
      underlay_(std::move(underlay)) {}

auto Deko3dPlasma::Target::whole(const DkImageView& view, std::uint32_t width,
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

auto Deko3dPlasma::Target::within(const DkImageView& view,
                                  const provider::SurfaceFrame& frame)
    -> Target {
  return Target{
      .view = view, .viewport = frame.viewport, .scissor = frame.scissor};
}

auto Deko3dPlasma::create(provider::horizon::Deko3dGpu& gpu,
                          StageControl& control)
    -> std::expected<Deko3dPlasma, std::string> {
  using provider::horizon::Deko3dMemory;
  using provider::horizon::Deko3dMemoryKind;
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
    return std::unexpected(std::string{"the plasma could not get GPU memory"});
  }
  const std::span<std::byte> target = code->cpu_bytes();
  for (std::size_t index = 0; index < sources.size(); ++index) {
    std::ranges::copy(std::as_bytes(sources[index]),
                      target.subspan(offsets[index]).begin());
  }
  Program program{
      .vertex = {}, .fragment = {}, .uniforms = uniforms->gpu_address()};
  const std::array<std::reference_wrapper<dk::Shader>, 2> outputs{
      program.vertex, program.fragment};
  for (std::size_t index = 0; index < outputs.size(); ++index) {
    dk::ShaderMaker{code->memblock(), code->offset() + offsets[index]}
        .initialize(outputs[index].get());
    if (!outputs[index].get().isValid()) {
      return std::unexpected(
          std::string{"the plasma shaders could not be loaded"});
    }
  }
  provider::FrameHook hook = gpu.add_frame_hook(
      provider::FrameStage::before_ui,
      [program, &control](dk::CmdBuf& commands, const FrameTarget& frame,
                          const provider::FrameInfo& info) {
        if (control.page() != StageControl::Page::stage) {
          return;
        }
        draw(commands, Target::whole(frame.view, frame.width, frame.height),
             program, control.advance(info));
      },
      StageControl::playing_options);
  const auto surface = [&gpu, program, &control](provider::SurfaceMode mode) {
    return gpu.add_surface(
        provider::SurfaceOptions{.mode = mode},
        [program, &control](dk::CmdBuf& commands, const FrameTarget& frame,
                            const provider::SurfaceFrame& placed) {
          draw(commands, Target::within(frame.view, placed), program,
               control.advance(placed));
        });
  };
  auto texture = surface(provider::SurfaceMode::texture);
  auto underlay = surface(provider::SurfaceMode::underlay);
  if (!texture.has_value() || !underlay.has_value()) {
    return std::unexpected(std::string{"the plasma surfaces were refused"});
  }
  return Deko3dPlasma{
      std::move(*code), std::move(*uniforms), program,
      std::move(hook),  std::move(*texture),  std::move(*underlay)};
}

void Deko3dPlasma::draw(dk::CmdBuf& commands, const Target& target,
                        const Program& program,
                        const StageControl::Uniforms& uniforms) {
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
  commands.bindUniformBuffer(DkStage_Fragment, 0, program.uniforms,
                             uniform_size);
  commands.pushConstants(program.uniforms, uniform_size, 0, sizeof(uniforms),
                         &uniforms);
  commands.draw(DkPrimitive_Triangles, 3, 1, 0, 0);
}

}
