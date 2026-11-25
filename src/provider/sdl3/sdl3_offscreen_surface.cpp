#include <arisa/provider/sdl3/sdl3_offscreen_surface.hpp>

namespace arisa::provider::sdl3 {

Sdl3OffscreenSurface::Sdl3OffscreenSurface(Sdl3EventPump& pump,
                                           SurfaceSize size) noexcept
    : pump_(pump), size_(size) {}

auto Sdl3OffscreenSurface::create(Sdl3EventPump& pump, SurfaceSize size)
    -> std::expected<Sdl3OffscreenSurface, ProviderError> {
  if (size.empty()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  return Sdl3OffscreenSurface{pump, size};
}

auto Sdl3OffscreenSurface::size() const noexcept -> SurfaceSize {
  return size_;
}

auto Sdl3OffscreenSurface::scale() const noexcept -> SurfaceScale {
  return SurfaceScale::for_pixels(size_);
}

auto Sdl3OffscreenSurface::poll() -> std::optional<SurfaceEvent> {
  pump_.get().pump();
  return std::nullopt;
}

auto Sdl3OffscreenSurface::display_mode() const noexcept -> DisplayMode {
  return DisplayMode::desktop;
}

}
