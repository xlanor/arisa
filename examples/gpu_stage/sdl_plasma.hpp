#pragma once

#include <stage_control.hpp>

#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/render_pass.hpp>
#include <arisa/provider/gpu/surface_frame.hpp>
#include <arisa/provider/sdl3/sdl3_gpu.hpp>
#include <arisa/support/unique_handle.hpp>

#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <expected>
#include <span>
#include <string>

namespace arisa::examples {

class SdlPlasma {
 public:
  [[nodiscard]] static auto create(provider::sdl3::Sdl3Gpu& gpu,
                                   StageControl& control)
      -> std::expected<SdlPlasma, std::string>;

  auto hook() const noexcept -> const provider::FrameHook& { return hook_; }

  auto texture_surface() const noexcept -> const provider::GpuSurface& {
    return texture_;
  }

  auto underlay_surface() const noexcept -> const provider::GpuSurface& {
    return underlay_;
  }

  struct Pipeline {
    SDL_GPUDevice* device;
    SDL_GPUGraphicsPipeline* pipeline;
  };

  struct PipelineRelease {
    using handle_type = Pipeline;

    static auto empty() noexcept -> handle_type {
      return Pipeline{.device = nullptr, .pipeline = nullptr};
    }

    static auto is_valid(const handle_type& handle) noexcept -> bool {
      return handle.pipeline != nullptr;
    }

    static void release(handle_type& handle) noexcept {
      SDL_ReleaseGPUGraphicsPipeline(handle.device, handle.pipeline);
    }
  };

  struct Shader {
    SDL_GPUDevice* device;
    SDL_GPUShader* shader;
  };

  struct ShaderRelease {
    using handle_type = Shader;

    static auto empty() noexcept -> handle_type {
      return Shader{.device = nullptr, .shader = nullptr};
    }

    static auto is_valid(const handle_type& handle) noexcept -> bool {
      return handle.shader != nullptr;
    }

    static void release(handle_type& handle) noexcept {
      SDL_ReleaseGPUShader(handle.device, handle.shader);
    }
  };

  using PipelineHandle = support::UniqueHandle<PipelineRelease>;
  using ShaderHandle = support::UniqueHandle<ShaderRelease>;

  struct Target {
    SDL_GPUCommandBuffer* commands;
    SDL_GPUTexture* texture;
    provider::PassRegion viewport;
    provider::ClipRect scissor;
    bool cycle;
    bool keep;

    static auto whole(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* texture,
                      std::uint32_t width, std::uint32_t height, bool cycle)
        -> Target;

    static auto within(const provider::sdl3::Sdl3Gpu::FrameTarget& target,
                       const provider::SurfaceFrame& frame) -> Target;
  };

  [[nodiscard]] static auto pipeline(SDL_GPUDevice* device,
                                     SDL_GPUTextureFormat format)
      -> std::expected<PipelineHandle, std::string>;

  static void draw(SDL_GPUGraphicsPipeline* pipeline, const Target& target,
                   const StageControl::Uniforms& uniforms);

 private:
  SdlPlasma(PipelineHandle pipeline, provider::FrameHook hook,
            provider::GpuSurface texture,
            provider::GpuSurface underlay) noexcept;

  static auto shader(SDL_GPUDevice* device, std::span<const std::uint8_t> code,
                     SDL_GPUShaderStage stage, std::uint32_t uniforms)
      -> ShaderHandle;

  PipelineHandle pipeline_;
  provider::FrameHook hook_;
  provider::GpuSurface texture_;
  provider::GpuSurface underlay_;
};

}
