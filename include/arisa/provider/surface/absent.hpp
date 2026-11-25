#pragma once

#include <arisa/provider/surface/provider.hpp>

namespace arisa::provider {

class AbsentSurfaceProvider final : public SurfaceProvider {
 public:
  auto size() const noexcept -> SurfaceSize override;
  auto scale() const noexcept -> SurfaceScale override;

  auto display_mode() const noexcept -> DisplayMode override;
  auto poll() -> std::optional<SurfaceEvent> override;
};

}
