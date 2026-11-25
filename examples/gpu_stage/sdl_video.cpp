#include <sdl_video.hpp>

#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <SDL3/SDL_error.h>

#include <utility>

namespace arisa::examples {

SdlVideo::SdlVideo(SDL_GPUDevice* device, SdlPlasma::PipelineHandle pipeline,
                   std::vector<Frame> ring) noexcept
    : device_(device), pipeline_(std::move(pipeline)), ring_(std::move(ring)) {}

auto SdlVideo::create(provider::sdl3::Sdl3Gpu& gpu)
    -> std::expected<SdlVideo, std::string> {
  SDL_GPUDevice* const device = gpu.device();
  auto pipeline = SdlPlasma::pipeline(device, format);
  if (!pipeline.has_value()) {
    return std::unexpected(pipeline.error());
  }
  std::vector<Frame> ring;
  ring.reserve(ring_size);
  for (std::size_t index = 0; index < ring_size; ++index) {
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage =
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    SDL_GPUTexture* const image = SDL_CreateGPUTexture(device, &info);
    if (image == nullptr) {
      return std::unexpected(std::string{"a video frame could not be made: "} +
                             SDL_GetError());
    }
    auto imported = gpu.import_texture(
        image,
        provider::ImportDescription{.width = width,
                                    .height = height,
                                    .format = provider::TextureFormat::rgba8},
        support::ScopedCallback{[device, image]() noexcept {
          SDL_ReleaseGPUTexture(device, image);
        }});
    if (!imported.has_value()) {
      return std::unexpected(
          std::string{"a video frame could not be imported"});
    }
    ring.push_back(Frame{.image = image, .texture = std::move(*imported)});
  }
  return SdlVideo{device, std::move(*pipeline), std::move(ring)};
}

auto SdlVideo::produce(StageControl& control, std::chrono::nanoseconds now)
    -> bool {
  Frame& frame = ring_[next_ % ring_.size()];
  if (frame.texture.in_use()) {
    if (next_ > 0) {
      ring_[(next_ - 1) % ring_.size()].texture.publish();
    }
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
  SDL_GPUCommandBuffer* const commands = SDL_AcquireGPUCommandBuffer(device_);
  if (commands == nullptr) {
    return false;
  }
  SdlPlasma::draw(
      pipeline_.get().pipeline,
      SdlPlasma::Target::whole(commands, frame.image, width, height, true),
      uniforms);
  if (!SDL_SubmitGPUCommandBuffer(commands)) {
    return false;
  }
  frame.texture.publish();
  control.show(frame.texture.id());
  ++next_;
  return true;
}

}
