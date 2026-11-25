#pragma once

#include <sdl_plasma.hpp>
#include <stage_control.hpp>

#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/sdl3/sdl3_gpu.hpp>

#include <SDL3/SDL_gpu.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::examples {

class SdlNv12Video {
 public:
  static constexpr std::size_t ring_size = 3;
  static constexpr std::uint32_t width = 640;
  static constexpr std::uint32_t height = 360;
  static constexpr SDL_GPUTextureFormat luma_format =
      SDL_GPU_TEXTUREFORMAT_R8_UNORM;
  static constexpr SDL_GPUTextureFormat chroma_format =
      SDL_GPU_TEXTUREFORMAT_R8G8_UNORM;

  [[nodiscard]] static auto create(provider::sdl3::Sdl3Gpu& gpu,
                                   const StageControl& control)
      -> std::expected<SdlNv12Video, std::string>;

  auto produce(StageControl& control, std::chrono::nanoseconds now) -> bool;

 private:
  class Planes {
   public:
    static auto create(SDL_GPUDevice* device)
        -> std::expected<std::shared_ptr<Planes>, std::string>;

    Planes(SDL_GPUDevice* device, SDL_GPUTexture* luma,
           SDL_GPUTexture* chroma) noexcept;
    Planes(const Planes&) = delete;
    auto operator=(const Planes&) -> Planes& = delete;
    Planes(Planes&&) = delete;
    auto operator=(Planes&&) -> Planes& = delete;
    ~Planes();

    auto luma() const noexcept -> SDL_GPUTexture* { return luma_; }

    auto chroma() const noexcept -> SDL_GPUTexture* { return chroma_; }

    auto natives() const noexcept -> std::array<SDL_GPUTexture*, 2> {
      return {luma_, chroma_};
    }

   private:
    static auto texture(SDL_GPUDevice* device, SDL_GPUTextureFormat format,
                        std::uint32_t texture_width,
                        std::uint32_t texture_height) -> SDL_GPUTexture*;

    SDL_GPUDevice* device_;
    SDL_GPUTexture* luma_;
    SDL_GPUTexture* chroma_;
  };

  struct Frame {
    std::shared_ptr<Planes> planes;
    provider::ImportDescription description;
    provider::sdl3::Sdl3Gpu::Imported texture;
  };

  SdlNv12Video(provider::sdl3::Sdl3Gpu& gpu,
               SdlPlasma::PipelineHandle luma_pipeline,
               SdlPlasma::PipelineHandle chroma_pipeline,
               std::vector<Frame> ring) noexcept;

  static auto import(provider::sdl3::Sdl3Gpu& gpu,
                     const std::shared_ptr<Planes>& planes,
                     const provider::ImportDescription& description)
      -> std::optional<provider::sdl3::Sdl3Gpu::Imported>;

  std::reference_wrapper<provider::sdl3::Sdl3Gpu> gpu_;
  SdlPlasma::PipelineHandle luma_pipeline_;
  SdlPlasma::PipelineHandle chroma_pipeline_;
  std::vector<Frame> ring_;
  std::size_t next_{0};
};

}
