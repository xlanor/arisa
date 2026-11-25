#include <arisa/provider/surface/absent.hpp>

namespace arisa::provider {

auto AbsentSurfaceProvider::size() const noexcept -> SurfaceSize { return {}; }

auto AbsentSurfaceProvider::scale() const noexcept -> SurfaceScale {
  return SurfaceScale::unit();
}

auto AbsentSurfaceProvider::poll() -> std::optional<SurfaceEvent> {
  return std::nullopt;
}

auto AbsentSurfaceProvider::display_mode() const noexcept -> DisplayMode {
  return DisplayMode::desktop;
}

}
