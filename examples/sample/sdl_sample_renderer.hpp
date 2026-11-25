#pragma once

#include <animation.hpp>
#include <sample_renderer.hpp>

#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/render_pass.hpp>
#include <arisa/provider/sdl3/sdl3_gpu.hpp>
#include <arisa/support/unique_handle.hpp>

#include <SDL3/SDL_gpu.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace arisa::examples {

class SdlSampleRenderer final : public SampleRenderer {
 public:
  static constexpr std::uint32_t picture_width = 480;
  static constexpr std::uint32_t picture_height = 270;
  static constexpr std::size_t ring_size = 3;
  static constexpr SDL_GPUTextureFormat picture_format =
      SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

  [[nodiscard]] static auto create(provider::sdl3::Sdl3Gpu& gpu,
                                   Animation& animation)
      -> std::expected<SdlSampleRenderer, std::string>;

  auto backdrop() const noexcept -> const provider::FrameHook& override {
    return backdrop_;
  }

  auto card() const noexcept -> const provider::GpuSurface& override {
    return card_;
  }

  auto picture() const noexcept -> provider::TextureId override {
    return ring_[shown_].texture.id();
  }

  auto repaint_picture() -> std::optional<provider::TextureId> override;

 private:
  template <class Object, void (*Release)(SDL_GPUDevice*, Object*)>
  struct DeviceRelease {
    struct handle_type {
      SDL_GPUDevice* device{nullptr};
      Object* object{nullptr};
    };

    static auto empty() noexcept -> handle_type { return {}; }

    static auto is_valid(const handle_type& handle) noexcept -> bool {
      return handle.object != nullptr;
    }

    static void release(handle_type& handle) noexcept {
      Release(handle.device, handle.object);
    }
  };

  using Pipeline = support::UniqueHandle<
      DeviceRelease<SDL_GPUGraphicsPipeline, SDL_ReleaseGPUGraphicsPipeline>>;
  using Shader =
      support::UniqueHandle<DeviceRelease<SDL_GPUShader, SDL_ReleaseGPUShader>>;

  struct Frame {
    SDL_GPUTexture* image;
    provider::sdl3::Sdl3Gpu::Imported texture;
  };

  struct Target {
    SDL_GPUCommandBuffer* commands;
    SDL_GPUTexture* texture;
    provider::PassRegion viewport;
    provider::ClipRect scissor;
    bool cycle;
  };

  SdlSampleRenderer(SDL_GPUDevice* device, Animation& animation,
                    Pipeline screen, Pipeline picture_pipeline,
                    std::vector<Frame> ring, provider::FrameHook backdrop,
                    provider::GpuSurface card) noexcept;

  static auto shader(SDL_GPUDevice* device, std::span<const std::uint8_t> code,
                     SDL_GPUShaderStage stage, std::uint32_t uniforms)
      -> Shader;

  [[nodiscard]] static auto frame(provider::sdl3::Sdl3Gpu& gpu)
      -> std::expected<Frame, std::string>;

  [[nodiscard]] static auto pipeline(SDL_GPUDevice* device,
                                     SDL_GPUTextureFormat format)
      -> std::expected<Pipeline, std::string>;

  static auto whole(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* texture,
                    std::uint32_t width, std::uint32_t height) -> Target;

  static void draw(SDL_GPUGraphicsPipeline* pipeline, const Target& target,
                   const Animation::Uniforms& uniforms);

  SDL_GPUDevice* device_;
  std::reference_wrapper<Animation> animation_;
  Pipeline screen_;
  Pipeline picture_pipeline_;
  std::vector<Frame> ring_;
  std::size_t shown_{0};
  provider::FrameHook backdrop_;
  provider::GpuSurface card_;
};

}
