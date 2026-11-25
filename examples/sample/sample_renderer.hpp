#pragma once

#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/texture.hpp>

#include <optional>

namespace arisa::examples {

class SampleRenderer {
 public:
  SampleRenderer() = default;
  SampleRenderer(const SampleRenderer&) = delete;
  auto operator=(const SampleRenderer&) -> SampleRenderer& = delete;
  SampleRenderer(SampleRenderer&&) = default;
  auto operator=(SampleRenderer&&) -> SampleRenderer& = delete;
  virtual ~SampleRenderer() = default;

  virtual auto backdrop() const noexcept -> const provider::FrameHook& = 0;

  virtual auto card() const noexcept -> const provider::GpuSurface& = 0;

  virtual auto picture() const noexcept -> provider::TextureId = 0;

  virtual auto repaint_picture() -> std::optional<provider::TextureId> = 0;
};

}
