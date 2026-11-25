#include <arisa/provider/headless/scripted_surface.hpp>

namespace arisa::provider::headless {

auto ScriptedSurface::size() const noexcept -> SurfaceSize {
  const std::scoped_lock lock{mutex_};
  return size_;
}

auto ScriptedSurface::scale() const noexcept -> SurfaceScale {
  const std::scoped_lock lock{mutex_};
  return scale_;
}

auto ScriptedSurface::poll() -> std::optional<SurfaceEvent> {
  const std::scoped_lock lock{mutex_};
  if (pending_.empty()) {
    return std::nullopt;
  }
  const SurfaceEvent event = pending_.front();
  pending_.pop_front();
  return event;
}

void ScriptedSurface::resize(SurfaceSize size) {
  const std::scoped_lock lock{mutex_};
  size_ = size;
  pending_.push_back(SurfaceEvent::resized);
}

void ScriptedSurface::set_scale(SurfaceScale scale) {
  const std::scoped_lock lock{mutex_};
  scale_ = scale;
  pending_.push_back(SurfaceEvent::scale_changed);
}

void ScriptedSurface::push(SurfaceEvent event) {
  const std::scoped_lock lock{mutex_};
  pending_.push_back(event);
}

auto ScriptedSurface::display_mode() const noexcept -> DisplayMode {
  const std::scoped_lock lock{mutex_};
  return mode_;
}

void ScriptedSurface::set_display_mode(DisplayMode mode) {
  const std::scoped_lock lock{mutex_};
  mode_ = mode;
  pending_.push_back(SurfaceEvent::operation_mode_changed);
}

}
