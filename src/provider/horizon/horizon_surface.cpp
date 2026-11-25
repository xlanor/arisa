#include <arisa/provider/horizon/horizon_surface.hpp>

#include <arisa/provider/horizon/console.hpp>

namespace arisa::provider::horizon {

void HorizonSurface::WindowRelease::release(handle_type& window) noexcept {
  Console::release_window(window);
}

HorizonSurface::~HorizonSurface() {
#if defined(__SWITCH2__)
  if (releases_layer_ && window_.valid()) {
    static_cast<void>(nwindowReleaseVi2Layer(window_.get()));
  }
#endif
}

HorizonSurface::HorizonSurface(NWindow* window,
                               AppletOperationMode mode) noexcept
    : window_(window),
      reported_mode_(mode),
      reported_size_(Console::framebuffer(window, mode)) {}

auto HorizonSurface::create(std::optional<SurfaceSize> docked_cap)
    -> std::expected<HorizonSurface, ProviderError> {
  NWindow* const window = nwindowGetDefault();
  if (window == nullptr || !nwindowIsValid(window)) {
    return std::unexpected(ProviderError{ProviderErrorCode::platform_failure});
  }
  if (auto configured = Console::configure_window(window, docked_cap);
      !configured.has_value()) {
    return std::unexpected(configured.error());
  }
  return HorizonSurface{window, appletGetOperationMode()};
}

auto HorizonSurface::size() const noexcept -> SurfaceSize {
  return Console::framebuffer(window_.get(), appletGetOperationMode());
}

auto HorizonSurface::scale() const noexcept -> SurfaceScale {
  return SurfaceScale::for_pixels(size());
}

auto HorizonSurface::poll() -> std::optional<SurfaceEvent> {
  const AppletOperationMode current = appletGetOperationMode();
  const SurfaceSize after = Console::framebuffer(window_.get(), current);
  if (current != reported_mode_) {
    reported_mode_ = current;
    pending_.push_back(SurfaceEvent::operation_mode_changed);
  }
  if (after != reported_size_) {
    const SurfaceSize before = reported_size_;
    reported_size_ = after;
    pending_.push_back(SurfaceEvent::resized);
    if (SurfaceScale::for_pixels(before) != SurfaceScale::for_pixels(after)) {
      pending_.push_back(SurfaceEvent::scale_changed);
    }
  }
  if (pending_.empty()) {
    return std::nullopt;
  }
  const SurfaceEvent event = pending_.front();
  pending_.pop_front();
  return event;
}

auto HorizonSurface::window() const noexcept -> NWindow* {
  return window_.get();
}

auto HorizonSurface::display_mode() const noexcept -> DisplayMode {
  return reported_mode_ == AppletOperationMode_Console ? DisplayMode::docked
                                                       : DisplayMode::handheld;
}

}
