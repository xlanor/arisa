#include <arisa/provider/sdl3/sdl3_lifecycle.hpp>

namespace arisa::provider::sdl3 {

Sdl3Lifecycle::Sdl3Lifecycle(Sdl3EventPump& pump)
    : pump_(pump), events_(pump.subscribe(Sdl3EventCategory::lifecycle)) {}

auto Sdl3Lifecycle::phase() const noexcept -> ScenePhase {
  if (background_) {
    return ScenePhase::background;
  }
  return focused_ ? ScenePhase::active : ScenePhase::inactive;
}

auto Sdl3Lifecycle::poll() -> std::optional<LifecycleEvent> {
  pump_.get().pump();
  while (const std::optional<SDL_Event> event = events_.next()) {
    if (const std::optional<LifecycleEvent> translated = translate(*event)) {
      track(*translated);
      return translated;
    }
  }
  return std::nullopt;
}

auto Sdl3Lifecycle::block_home_button()
    -> std::expected<HomeButtonBlock, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

auto Sdl3Lifecycle::translate(const SDL_Event& event) noexcept
    -> std::optional<LifecycleEvent> {
  switch (static_cast<SDL_EventType>(event.type)) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_TERMINATING:
      return LifecycleEvent::exit_requested;
    case SDL_EVENT_DID_ENTER_BACKGROUND:
      return LifecycleEvent::suspending;
    case SDL_EVENT_DID_ENTER_FOREGROUND:
      return LifecycleEvent::resumed;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
      return LifecycleEvent::focus_gained;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
      return LifecycleEvent::focus_lost;
    default:
      return std::nullopt;
  }
}

void Sdl3Lifecycle::track(LifecycleEvent event) noexcept {
  switch (event) {
    case LifecycleEvent::focus_gained:
      focused_ = true;
      break;
    case LifecycleEvent::focus_lost:
      focused_ = false;
      break;
    case LifecycleEvent::suspending:
      background_ = true;
      break;
    case LifecycleEvent::resumed:
      background_ = false;
      break;
    case LifecycleEvent::exit_requested:
    case LifecycleEvent::operation_mode_changed:
      break;
  }
}

}
