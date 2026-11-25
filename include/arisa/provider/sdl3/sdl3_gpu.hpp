#pragma once

#include <arisa/provider/gpu/external_texture.hpp>
#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/frame_hooks.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/gpu_surfaces.hpp>
#include <arisa/provider/gpu/provider.hpp>
#include <arisa/provider/sdl3/sdl3_offscreen_surface.hpp>
#include <arisa/provider/sdl3/sdl3_window_surface.hpp>

#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace arisa::provider::sdl3 {

class Sdl3Gpu final : public GpuProvider {
 public:
  struct FrameTarget {
    SDL_GPUCommandBuffer* commands;
    SDL_GPUTexture* texture;
    SDL_GPUTextureFormat format;
    std::uint32_t width;
    std::uint32_t height;
  };

  using Hooks = FrameHooks<const FrameTarget&>;
  using FrameHookFunction = Hooks::Function;
  using Imported = ExternalTexture<>;
  using Surfaces = GpuSurfaces<const FrameTarget&>;
  using SurfaceFunction = Surfaces::Function;

  static constexpr SDL_GPUShaderFormat shader_formats =
      SDL_GPU_SHADERFORMAT_SPIRV;

  [[nodiscard]] static auto create(Sdl3WindowSurface& surface)
      -> std::expected<Sdl3Gpu, ProviderError>;

  [[nodiscard]] static auto create(const Sdl3OffscreenSurface& surface)
      -> std::expected<Sdl3Gpu, ProviderError>;

  [[nodiscard]] auto create_texture(TextureDescription description)
      -> std::expected<Texture, ProviderError> override;

  [[nodiscard]] auto update_texture(Texture& texture, TextureRegion region,
                                    PixelRows pixels)
      -> std::expected<void, ProviderError> override;

  [[nodiscard]] auto import_texture(SDL_GPUTexture* texture,
                                    ImportDescription description,
                                    support::ScopedCallback released = {})
      -> std::expected<Imported, ProviderError>;

  [[nodiscard]] auto import_texture(std::span<SDL_GPUTexture* const> planes,
                                    ImportDescription description,
                                    support::ScopedCallback released = {})
      -> std::expected<Imported, ProviderError>;

  [[nodiscard]] auto begin_frame()
      -> std::expected<GpuFrame, ProviderError> override;

  [[nodiscard]] auto capture() -> std::expected<Image, ProviderError> override;

  auto timings() const -> GpuTimings override;

  auto inventory() const -> GpuInventory override;

  auto frame_schedule() noexcept
      -> std::optional<std::reference_wrapper<FrameSchedule>> override;

  auto publications() const noexcept -> std::optional<
      std::reference_wrapper<const TexturePublications>> override;

  auto add_frame_hook(FrameStage stage, FrameHookFunction function,
                      FrameHookOptions options = {}) -> FrameHook;

  [[nodiscard]] auto add_surface(SurfaceOptions options,
                                 SurfaceFunction function)
      -> std::expected<GpuSurface, ProviderError>;

  auto surfaces() noexcept
      -> std::optional<std::reference_wrapper<SurfaceDirectory>> override;

  auto render_queue() noexcept
      -> std::optional<std::reference_wrapper<RenderQueue>> override;

  auto device() const noexcept -> SDL_GPUDevice*;

  auto driver() const noexcept -> std::string_view;

  auto target_format() const noexcept -> SDL_GPUTextureFormat;

  auto live_textures() const -> std::size_t;

 private:
  template <class Resource, auto Release>
  struct DeviceResource;

  class Device;
  class Frame;

  explicit Sdl3Gpu(std::shared_ptr<Device> device) noexcept;

  std::shared_ptr<Device> device_;
  std::shared_ptr<Frame> spare_;
};

}
