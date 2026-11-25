#include <sdl_nv12_video.hpp>

#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <SDL3/SDL_error.h>

#include <span>
#include <utility>

namespace arisa::examples {

SdlNv12Video::Planes::Planes(SDL_GPUDevice* device, SDL_GPUTexture* luma,
                             SDL_GPUTexture* chroma) noexcept
    : device_(device), luma_(luma), chroma_(chroma) {}

SdlNv12Video::Planes::~Planes() {
  if (luma_ != nullptr) {
    SDL_ReleaseGPUTexture(device_, luma_);
  }
  if (chroma_ != nullptr) {
    SDL_ReleaseGPUTexture(device_, chroma_);
  }
}

auto SdlNv12Video::Planes::texture(SDL_GPUDevice* device,
                                   SDL_GPUTextureFormat format,
                                   std::uint32_t texture_width,
                                   std::uint32_t texture_height)
    -> SDL_GPUTexture* {
  SDL_GPUTextureCreateInfo info{};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = format;
  info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
  info.width = texture_width;
  info.height = texture_height;
  info.layer_count_or_depth = 1;
  info.num_levels = 1;
  return SDL_CreateGPUTexture(device, &info);
}

auto SdlNv12Video::Planes::create(SDL_GPUDevice* device)
    -> std::expected<std::shared_ptr<Planes>, std::string> {
  const provider::ImportDescription layout{
      .width = width,
      .height = height,
      .format = provider::TextureFormat::nv12};
  const provider::TextureDescription chroma_plane = layout.plane(1);
  auto made = std::make_shared<Planes>(
      device, texture(device, luma_format, width, height),
      texture(device, chroma_format, chroma_plane.width, chroma_plane.height));
  if (made->luma() == nullptr || made->chroma() == nullptr) {
    return std::unexpected(std::string{"an NV12 plane could not be made: "} +
                           SDL_GetError());
  }
  return made;
}

SdlNv12Video::SdlNv12Video(provider::sdl3::Sdl3Gpu& gpu,
                           SdlPlasma::PipelineHandle luma_pipeline,
                           SdlPlasma::PipelineHandle chroma_pipeline,
                           std::vector<Frame> ring) noexcept
    : gpu_(gpu),
      luma_pipeline_(std::move(luma_pipeline)),
      chroma_pipeline_(std::move(chroma_pipeline)),
      ring_(std::move(ring)) {}

auto SdlNv12Video::import(provider::sdl3::Sdl3Gpu& gpu,
                          const std::shared_ptr<Planes>& planes,
                          const provider::ImportDescription& description)
    -> std::optional<provider::sdl3::Sdl3Gpu::Imported> {
  const std::array<SDL_GPUTexture*, 2> natives = planes->natives();
  auto imported =
      gpu.import_texture(std::span<SDL_GPUTexture* const>{natives}, description,
                         support::ScopedCallback{[held = planes]() noexcept {
                           static_cast<void>(held);
                         }});
  if (!imported.has_value()) {
    return std::nullopt;
  }
  return std::move(*imported);
}

auto SdlNv12Video::create(provider::sdl3::Sdl3Gpu& gpu,
                          const StageControl& control)
    -> std::expected<SdlNv12Video, std::string> {
  SDL_GPUDevice* const device = gpu.device();
  auto luma_pipeline = SdlPlasma::pipeline(device, luma_format);
  if (!luma_pipeline.has_value()) {
    return std::unexpected(luma_pipeline.error());
  }
  auto chroma_pipeline = SdlPlasma::pipeline(device, chroma_format);
  if (!chroma_pipeline.has_value()) {
    return std::unexpected(chroma_pipeline.error());
  }
  const provider::ImportDescription description =
      control.declared(width, height);
  std::vector<Frame> ring;
  ring.reserve(ring_size);
  for (std::size_t index = 0; index < ring_size; ++index) {
    auto planes = Planes::create(device);
    if (!planes.has_value()) {
      return std::unexpected(planes.error());
    }
    auto imported = import(gpu, *planes, description);
    if (!imported.has_value()) {
      return std::unexpected(
          std::string{"an NV12 frame could not be imported"});
    }
    ring.push_back(Frame{.planes = std::move(*planes),
                         .description = description,
                         .texture = std::move(*imported)});
  }
  return SdlNv12Video{gpu, std::move(*luma_pipeline),
                      std::move(*chroma_pipeline), std::move(ring)};
}

auto SdlNv12Video::produce(StageControl& control, std::chrono::nanoseconds now)
    -> bool {
  Frame& frame = ring_[next_ % ring_.size()];
  if (frame.texture.in_use()) {
    if (next_ > 0) {
      ring_[(next_ - 1) % ring_.size()].texture.publish();
    }
    return false;
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
  SDL_GPUCommandBuffer* const commands =
      SDL_AcquireGPUCommandBuffer(gpu_.get().device());
  if (commands == nullptr) {
    return false;
  }
  const provider::TextureDescription chroma = wanted.plane(1);
  SdlPlasma::draw(luma_pipeline_.get().pipeline,
                  SdlPlasma::Target::whole(commands, frame.planes->luma(),
                                           width, height, true),
                  uniforms.encoding(StageControl::Plane::luma, width));
  SdlPlasma::draw(chroma_pipeline_.get().pipeline,
                  SdlPlasma::Target::whole(commands, frame.planes->chroma(),
                                           chroma.width, chroma.height, true),
                  uniforms.encoding(StageControl::Plane::chroma, width));
  if (!SDL_SubmitGPUCommandBuffer(commands)) {
    return false;
  }
  frame.texture.publish();
  control.show(frame.texture.id());
  ++next_;
  return true;
}

}
