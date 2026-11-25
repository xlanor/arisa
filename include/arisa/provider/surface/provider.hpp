#pragma once

#include <arisa/provider/surface/display_mode.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>

#include <cstdint>
#include <optional>

namespace arisa::provider {

enum class SurfaceEvent : std::uint8_t {
  resized,
  scale_changed,
  operation_mode_changed,
};

class SurfaceProvider {
 public:
  virtual ~SurfaceProvider() = default;

  virtual auto size() const noexcept -> SurfaceSize = 0;

  virtual auto scale() const noexcept -> SurfaceScale = 0;

  virtual auto display_mode() const noexcept -> DisplayMode = 0;

  virtual auto poll() -> std::optional<SurfaceEvent> = 0;
};

}
