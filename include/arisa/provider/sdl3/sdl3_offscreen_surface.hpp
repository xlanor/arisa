#pragma once

#include <arisa/provider/sdl3/sdl3_event_pump.hpp>
#include <arisa/provider/surface/provider.hpp>

#include <expected>
#include <functional>
#include <optional>

namespace arisa::provider::sdl3 {

class Sdl3OffscreenSurface final : public SurfaceProvider {
 public:
  [[nodiscard]] static auto create(Sdl3EventPump& pump, SurfaceSize size)
      -> std::expected<Sdl3OffscreenSurface, ProviderError>;

  auto size() const noexcept -> SurfaceSize override;

  auto scale() const noexcept -> SurfaceScale override;

  auto display_mode() const noexcept -> DisplayMode override;

  auto poll() -> std::optional<SurfaceEvent> override;

 private:
  Sdl3OffscreenSurface(Sdl3EventPump& pump, SurfaceSize size) noexcept;

  std::reference_wrapper<Sdl3EventPump> pump_;
  SurfaceSize size_;
};

}
