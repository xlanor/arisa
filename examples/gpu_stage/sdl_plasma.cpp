#include <plasma_shaders.hpp>
#include <sdl_plasma.hpp>

#include <SDL3/SDL_error.h>

#include <utility>

namespace arisa::examples {

SdlPlasma::SdlPlasma(PipelineHandle pipeline, provider::FrameHook hook,
                     provider::GpuSurface texture,
                     provider::GpuSurface underlay) noexcept
    : pipeline_(std::move(pipeline)),
      hook_(std::move(hook)),
      texture_(std::move(texture)),
      underlay_(std::move(underlay)) {}

auto SdlPlasma::Target::whole(SDL_GPUCommandBuffer* commands,
                              SDL_GPUTexture* texture, std::uint32_t width,
                              std::uint32_t height, bool cycle) -> Target {
  return Target{
      .commands = commands,
      .texture = texture,
      .viewport = provider::PassRegion{.x = 0.0F,
                                       .y = 0.0F,
                                       .width = static_cast<float>(width),
                                       .height = static_cast<float>(height)},
      .scissor =
          provider::ClipRect{.x = 0, .y = 0, .width = width, .height = height},
      .cycle = cycle,
      .keep = false};
}

auto SdlPlasma::Target::within(
    const provider::sdl3::Sdl3Gpu::FrameTarget& target,
    const provider::SurfaceFrame& frame) -> Target {
  return Target{.commands = target.commands,
                .texture = target.texture,
                .viewport = frame.viewport,
                .scissor = frame.scissor,
                .cycle = false,
                .keep = frame.mode == provider::SurfaceMode::underlay};
}

auto SdlPlasma::shader(SDL_GPUDevice* device,
                       std::span<const std::uint8_t> code,
                       SDL_GPUShaderStage stage, std::uint32_t uniforms)
    -> ShaderHandle {
  SDL_GPUShaderCreateInfo info{};
  info.code_size = code.size();
  info.code = code.data();
  info.entrypoint = "main";
  info.format = SDL_GPU_SHADERFORMAT_SPIRV;
  info.stage = stage;
  info.num_uniform_buffers = uniforms;
  return ShaderHandle{
      Shader{.device = device, .shader = SDL_CreateGPUShader(device, &info)}};
}

auto SdlPlasma::pipeline(SDL_GPUDevice* device, SDL_GPUTextureFormat format)
    -> std::expected<PipelineHandle, std::string> {
  const ShaderHandle vertex =
      shader(device, PlasmaShaders::vertex(), SDL_GPU_SHADERSTAGE_VERTEX, 0);
  const ShaderHandle fragment = shader(device, PlasmaShaders::fragment(),
                                       SDL_GPU_SHADERSTAGE_FRAGMENT, 1);
  if (!vertex.valid() || !fragment.valid()) {
    return std::unexpected(std::string{"the plasma shaders did not compile: "} +
                           SDL_GetError());
  }
  SDL_GPUColorTargetDescription colour{};
  colour.format = format;
  SDL_GPUGraphicsPipelineCreateInfo layout{};
  layout.vertex_shader = vertex.get().shader;
  layout.fragment_shader = fragment.get().shader;
  layout.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  layout.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  layout.target_info.color_target_descriptions = &colour;
  layout.target_info.num_color_targets = 1;
  PipelineHandle made{
      Pipeline{.device = device,
               .pipeline = SDL_CreateGPUGraphicsPipeline(device, &layout)}};
  if (!made.valid()) {
    return std::unexpected(
        std::string{"the plasma pipeline could not be created: "} +
        SDL_GetError());
  }
  return made;
}

auto SdlPlasma::create(provider::sdl3::Sdl3Gpu& gpu, StageControl& control)
    -> std::expected<SdlPlasma, std::string> {
  auto made = pipeline(gpu.device(), gpu.target_format());
  if (!made.has_value()) {
    return std::unexpected(made.error());
  }
  provider::FrameHook hook = gpu.add_frame_hook(
      provider::FrameStage::before_ui,
      [raw = made->get().pipeline, &control](
          const provider::sdl3::Sdl3Gpu::FrameTarget& target,
          const provider::FrameInfo& info) {
        if (control.page() != StageControl::Page::stage) {
          return;
        }
        draw(raw,
             Target::whole(target.commands, target.texture, target.width,
                           target.height, false),
             control.advance(info));
      },
      StageControl::playing_options);
  const auto surface = [&gpu, raw = made->get().pipeline,
                        &control](provider::SurfaceMode mode) {
    return gpu.add_surface(
        provider::SurfaceOptions{.mode = mode},
        [raw, &control](const provider::sdl3::Sdl3Gpu::FrameTarget& target,
                        const provider::SurfaceFrame& frame) {
          draw(raw, Target::within(target, frame), control.advance(frame));
        });
  };
  auto texture = surface(provider::SurfaceMode::texture);
  auto underlay = surface(provider::SurfaceMode::underlay);
  if (!texture.has_value() || !underlay.has_value()) {
    return std::unexpected(std::string{"the plasma surfaces were refused"});
  }
  return SdlPlasma{std::move(*made), std::move(hook), std::move(*texture),
                   std::move(*underlay)};
}

void SdlPlasma::draw(SDL_GPUGraphicsPipeline* pipeline, const Target& target,
                     const StageControl::Uniforms& uniforms) {
  SDL_PushGPUFragmentUniformData(target.commands, 0, &uniforms,
                                 sizeof(uniforms));
  SDL_GPUColorTargetInfo colour{};
  colour.texture = target.texture;
  colour.load_op = target.keep ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_DONT_CARE;
  colour.store_op = SDL_GPU_STOREOP_STORE;
  colour.cycle = target.cycle;
  SDL_GPURenderPass* const pass =
      SDL_BeginGPURenderPass(target.commands, &colour, 1, nullptr);
  if (pass == nullptr) {
    return;
  }
  const SDL_GPUViewport viewport{.x = target.viewport.x,
                                 .y = target.viewport.y,
                                 .w = target.viewport.width,
                                 .h = target.viewport.height,
                                 .min_depth = 0.0F,
                                 .max_depth = 1.0F};
  SDL_SetGPUViewport(pass, &viewport);
  const SDL_Rect scissor{.x = static_cast<int>(target.scissor.x),
                         .y = static_cast<int>(target.scissor.y),
                         .w = static_cast<int>(target.scissor.width),
                         .h = static_cast<int>(target.scissor.height)};
  SDL_SetGPUScissor(pass, &scissor);
  SDL_BindGPUGraphicsPipeline(pass, pipeline);
  SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
  SDL_EndGPURenderPass(pass);
}

}
