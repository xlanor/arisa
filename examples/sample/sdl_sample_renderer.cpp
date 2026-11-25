#include <plasma_shaders.hpp>
#include <sdl_sample_renderer.hpp>

#include <arisa/support/scoped_callback.hpp>

#include <SDL3/SDL_error.h>

#include <utility>

namespace arisa::examples {

using provider::sdl3::Sdl3Gpu;

SdlSampleRenderer::SdlSampleRenderer(SDL_GPUDevice* device,
                                     Animation& animation, Pipeline screen,
                                     Pipeline picture_pipeline,
                                     std::vector<Frame> ring,
                                     provider::FrameHook backdrop,
                                     provider::GpuSurface card) noexcept
    : device_(device),
      animation_(animation),
      screen_(std::move(screen)),
      picture_pipeline_(std::move(picture_pipeline)),
      ring_(std::move(ring)),
      backdrop_(std::move(backdrop)),
      card_(std::move(card)) {}

auto SdlSampleRenderer::create(Sdl3Gpu& gpu, Animation& animation)
    -> std::expected<SdlSampleRenderer, std::string> {
  SDL_GPUDevice* const device = gpu.device();
  auto screen = pipeline(device, gpu.target_format());
  if (!screen.has_value()) {
    return std::unexpected(screen.error());
  }
  auto picture_pipeline = pipeline(device, picture_format);
  if (!picture_pipeline.has_value()) {
    return std::unexpected(picture_pipeline.error());
  }
  std::vector<Frame> ring;
  for (std::size_t index = 0; index < ring_size; ++index) {
    auto made = frame(gpu);
    if (!made.has_value()) {
      return std::unexpected(made.error());
    }
    ring.push_back(std::move(*made));
  }
  provider::FrameHook backdrop = gpu.add_frame_hook(
      provider::FrameStage::before_ui,
      [raw = screen->get().object, &animation](
          const Sdl3Gpu::FrameTarget& target, const provider::FrameInfo& info) {
        draw(
            raw,
            whole(target.commands, target.texture, target.width, target.height),
            animation.advance(Animation::Pattern::plasma, info));
      },
      provider::FrameHookOptions{.drive = provider::FrameDrive::continuous});
  auto card = gpu.add_surface(
      provider::SurfaceOptions{.mode = provider::SurfaceMode::texture,
                               .drive = provider::FrameDrive::continuous},
      [raw = screen->get().object, &animation](
          const Sdl3Gpu::FrameTarget& target,
          const provider::SurfaceFrame& frame) {
        draw(raw,
             Target{.commands = target.commands,
                    .texture = target.texture,
                    .viewport = frame.viewport,
                    .scissor = frame.scissor,
                    .cycle = false},
             animation.advance(Animation::Pattern::ripples, frame.frame));
      });
  if (!card.has_value()) {
    return std::unexpected(std::string{"the card surface was refused"});
  }
  SdlSampleRenderer made{device,
                         animation,
                         std::move(*screen),
                         std::move(*picture_pipeline),
                         std::move(ring),
                         std::move(backdrop),
                         std::move(*card)};
  made.shown_ = ring_size - 1;
  static_cast<void>(made.repaint_picture());
  return made;
}

auto SdlSampleRenderer::frame(Sdl3Gpu& gpu)
    -> std::expected<Frame, std::string> {
  SDL_GPUDevice* const device = gpu.device();
  SDL_GPUTextureCreateInfo description{};
  description.type = SDL_GPU_TEXTURETYPE_2D;
  description.format = picture_format;
  description.usage =
      SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
  description.width = picture_width;
  description.height = picture_height;
  description.layer_count_or_depth = 1;
  description.num_levels = 1;
  SDL_GPUTexture* const image = SDL_CreateGPUTexture(device, &description);
  if (image == nullptr) {
    return std::unexpected(std::string{"the picture could not be made: "} +
                           SDL_GetError());
  }
  auto texture = gpu.import_texture(
      image,
      provider::ImportDescription{.width = picture_width,
                                  .height = picture_height,
                                  .format = provider::TextureFormat::rgba8},
      support::ScopedCallback{[device, image]() noexcept {
        SDL_ReleaseGPUTexture(device, image);
      }});
  if (!texture.has_value()) {
    return std::unexpected(std::string{"the picture could not be imported"});
  }
  return Frame{.image = image, .texture = std::move(*texture)};
}

auto SdlSampleRenderer::repaint_picture()
    -> std::optional<provider::TextureId> {
  const std::size_t next = (shown_ + 1) % ring_.size();
  Frame& frame = ring_[next];
  if (frame.texture.in_use()) {
    return std::nullopt;
  }
  SDL_GPUCommandBuffer* const commands = SDL_AcquireGPUCommandBuffer(device_);
  if (commands == nullptr) {
    return std::nullopt;
  }
  Target target = whole(commands, frame.image, picture_width, picture_height);
  target.cycle = true;
  draw(picture_pipeline_.get().object, target,
       animation_.get().latest(Animation::Pattern::tunnel, picture_width,
                               picture_height));
  if (!SDL_SubmitGPUCommandBuffer(commands)) {
    return std::nullopt;
  }
  frame.texture.publish();
  shown_ = next;
  return frame.texture.id();
}

auto SdlSampleRenderer::shader(SDL_GPUDevice* device,
                               std::span<const std::uint8_t> code,
                               SDL_GPUShaderStage stage, std::uint32_t uniforms)
    -> Shader {
  SDL_GPUShaderCreateInfo info{};
  info.code_size = code.size();
  info.code = code.data();
  info.entrypoint = "main";
  info.format = SDL_GPU_SHADERFORMAT_SPIRV;
  info.stage = stage;
  info.num_uniform_buffers = uniforms;
  return Shader{
      {.device = device, .object = SDL_CreateGPUShader(device, &info)}};
}

auto SdlSampleRenderer::pipeline(SDL_GPUDevice* device,
                                 SDL_GPUTextureFormat format)
    -> std::expected<Pipeline, std::string> {
  const Shader vertex =
      shader(device, PlasmaShaders::vertex(), SDL_GPU_SHADERSTAGE_VERTEX, 0);
  const Shader fragment = shader(device, PlasmaShaders::fragment(),
                                 SDL_GPU_SHADERSTAGE_FRAGMENT, 1);
  if (!vertex.valid() || !fragment.valid()) {
    return std::unexpected(std::string{"the shaders did not load: "} +
                           SDL_GetError());
  }
  SDL_GPUColorTargetDescription colour{};
  colour.format = format;
  SDL_GPUGraphicsPipelineCreateInfo info{};
  info.vertex_shader = vertex.get().object;
  info.fragment_shader = fragment.get().object;
  info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  info.target_info.color_target_descriptions = &colour;
  info.target_info.num_color_targets = 1;
  Pipeline made{{.device = device,
                 .object = SDL_CreateGPUGraphicsPipeline(device, &info)}};
  if (!made.valid()) {
    return std::unexpected(std::string{"the pipeline could not be made: "} +
                           SDL_GetError());
  }
  return made;
}

auto SdlSampleRenderer::whole(SDL_GPUCommandBuffer* commands,
                              SDL_GPUTexture* texture, std::uint32_t width,
                              std::uint32_t height) -> Target {
  return Target{
      .commands = commands,
      .texture = texture,
      .viewport = provider::PassRegion{.x = 0.0F,
                                       .y = 0.0F,
                                       .width = static_cast<float>(width),
                                       .height = static_cast<float>(height)},
      .scissor =
          provider::ClipRect{.x = 0, .y = 0, .width = width, .height = height},
      .cycle = false};
}

void SdlSampleRenderer::draw(SDL_GPUGraphicsPipeline* pipeline,
                             const Target& target,
                             const Animation::Uniforms& uniforms) {
  SDL_PushGPUFragmentUniformData(target.commands, 0, &uniforms,
                                 sizeof(uniforms));
  SDL_GPUColorTargetInfo colour{};
  colour.texture = target.texture;
  colour.load_op = SDL_GPU_LOADOP_DONT_CARE;
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
