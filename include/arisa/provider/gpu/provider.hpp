#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/frame_schedule.hpp>
#include <arisa/provider/gpu/gpu_frame.hpp>
#include <arisa/provider/gpu/gpu_inventory.hpp>
#include <arisa/provider/gpu/gpu_resources.hpp>
#include <arisa/provider/gpu/gpu_timings.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/gpu/pixel_rows.hpp>
#include <arisa/provider/gpu/render_queue.hpp>
#include <arisa/provider/gpu/surface_directory.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/gpu/texture_publications.hpp>

#include <cstddef>
#include <expected>
#include <functional>
#include <optional>
#include <span>

namespace arisa::provider {

class GpuProvider {
 public:
  virtual ~GpuProvider() = default;

  [[nodiscard]] virtual auto create_texture(TextureDescription description)
      -> std::expected<Texture, ProviderError> = 0;

  [[nodiscard]] virtual auto update_texture(Texture& texture,
                                            TextureRegion region,
                                            PixelRows pixels)
      -> std::expected<void, ProviderError> = 0;

  [[nodiscard]] virtual auto begin_frame()
      -> std::expected<GpuFrame, ProviderError> = 0;

  [[nodiscard]] virtual auto begin_replay()
      -> std::expected<GpuFrame, ProviderError> {
    return std::unexpected(ProviderError::unsupported_capability());
  }

  [[nodiscard]] virtual auto capture()
      -> std::expected<Image, ProviderError> = 0;

  virtual auto timings() const -> GpuTimings { return {}; }

  virtual auto resources() const -> GpuResources { return {}; }

  virtual auto inventory() const -> GpuInventory { return {}; }

  virtual auto presents_off_thread() const noexcept -> bool { return false; }

  virtual auto frame_schedule() noexcept
      -> std::optional<std::reference_wrapper<FrameSchedule>> {
    return std::nullopt;
  }

  virtual auto publications() const noexcept
      -> std::optional<std::reference_wrapper<const TexturePublications>> {
    return std::nullopt;
  }

  virtual auto surfaces() noexcept
      -> std::optional<std::reference_wrapper<SurfaceDirectory>> {
    return std::nullopt;
  }

  virtual auto render_queue() noexcept
      -> std::optional<std::reference_wrapper<RenderQueue>> {
    return std::nullopt;
  }
};

}
