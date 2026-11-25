#include <arisa/provider/sdl3/sdl3_window_surface.hpp>

#include <SDL3/SDL_keyboard.h>

#include <string>
#include <utility>

namespace arisa::provider::sdl3 {

Sdl3WindowSurface::Sdl3WindowSurface(Sdl3Context video, Window window,
                                     Sdl3EventPump& pump)
    : video_(std::move(video)),
      window_(std::move(window)),
      pump_(pump),
      events_(pump.subscribe(Sdl3EventCategory::window)),
      size_(measure(window_.get())),
      scale_(SurfaceScale::for_pixels(size_)) {}

auto Sdl3WindowSurface::create(Sdl3EventPump& pump, std::string_view title,
                               SurfaceSize size)
    -> std::expected<Sdl3WindowSurface, ProviderError> {
  if (size.empty()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  auto video = Sdl3Context::create(SDL_INIT_VIDEO);
  if (!video.has_value()) {
    return std::unexpected(video.error());
  }
  const std::string name{title};
  Window window{
      SDL_CreateWindow(name.c_str(), static_cast<int>(size.width.value()),
                       static_cast<int>(size.height.value()),
                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY)};
  if (!window.valid()) {
    return std::unexpected(ProviderError{ProviderErrorCode::platform_failure});
  }
  static_cast<void>(SDL_StartTextInput(window.get()));
  return Sdl3WindowSurface{std::move(*video), std::move(window), pump};
}

auto Sdl3WindowSurface::size() const noexcept -> SurfaceSize { return size_; }

auto Sdl3WindowSurface::scale() const noexcept -> SurfaceScale {
  return scale_;
}

auto Sdl3WindowSurface::poll() -> std::optional<SurfaceEvent> {
  pump_.get().pump();
  const SDL_WindowID id = SDL_GetWindowID(window_.get());
  bool changed = false;
  while (const std::optional<SDL_Event> event = events_.next()) {
    switch (static_cast<SDL_EventType>(event->type)) {
      case SDL_EVENT_WINDOW_RESIZED:
      case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
      case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
      case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        changed = changed || event->window.windowID == id;
        break;
      default:
        break;
    }
  }
  if (changed) {
    refresh();
  }
  if (pending_.empty()) {
    return std::nullopt;
  }
  const SurfaceEvent next = pending_.front();
  pending_.pop_front();
  return next;
}

void Sdl3WindowSurface::refresh() {
  const SurfaceSize measured = measure(window_.get());
  if (measured != size_) {
    size_ = measured;
    pending_.push_back(SurfaceEvent::resized);
  }
  const SurfaceScale rescaled = SurfaceScale::for_pixels(size_);
  if (rescaled != scale_) {
    scale_ = rescaled;
    pending_.push_back(SurfaceEvent::scale_changed);
  }
}

auto Sdl3WindowSurface::measure(SDL_Window* window) noexcept -> SurfaceSize {
  int width = 0;
  int height = 0;
  if (!SDL_GetWindowSizeInPixels(window, &width, &height) || width < 0 ||
      height < 0) {
    return {};
  }
  return SurfaceSize{
      .width = SurfaceWidth{static_cast<std::uint32_t>(width)},
      .height = SurfaceHeight{static_cast<std::uint32_t>(height)}};
}

void Sdl3WindowSurface::WindowRelease::release(handle_type& window) noexcept {
  SDL_StopTextInput(window);
  SDL_DestroyWindow(window);
}

auto Sdl3WindowSurface::display_mode() const noexcept -> DisplayMode {
  return DisplayMode::desktop;
}

}
