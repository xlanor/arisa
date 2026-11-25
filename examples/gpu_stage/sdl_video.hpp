#pragma once

#include <sdl_plasma.hpp>
#include <stage_control.hpp>

#include <arisa/provider/sdl3/sdl3_gpu.hpp>

#include <SDL3/SDL_gpu.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace arisa::examples {

class SdlVideo {
 public:
  static constexpr std::size_t ring_size = 3;
  static constexpr std::uint32_t width = 640;
  static constexpr std::uint32_t height = 360;
  static constexpr SDL_GPUTextureFormat format =
      SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

  [[nodiscard]] static auto create(provider::sdl3::Sdl3Gpu& gpu)
      -> std::expected<SdlVideo, std::string>;

  auto produce(StageControl& control, std::chrono::nanoseconds now) -> bool;

 private:
  struct Frame {
    SDL_GPUTexture* image;
    provider::sdl3::Sdl3Gpu::Imported texture;
  };

  SdlVideo(SDL_GPUDevice* device, SdlPlasma::PipelineHandle pipeline,
           std::vector<Frame> ring) noexcept;

  SDL_GPUDevice* device_;
  SdlPlasma::PipelineHandle pipeline_;
  std::vector<Frame> ring_;
  std::size_t next_{0};
};

}
