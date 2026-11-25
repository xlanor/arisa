#include <soak_lifecycle.hpp>

namespace arisa::examples {

auto SoakLifecycle::poll() -> std::optional<provider::LifecycleEvent> {
  const std::optional<provider::LifecycleEvent> event = inner_.get().poll();
  if (!event.has_value()) {
    return event;
  }
  switch (*event) {
    case provider::LifecycleEvent::suspending:
      ++suspensions_;
      break;
    case provider::LifecycleEvent::resumed:
      ++resumptions_;
      break;
    case provider::LifecycleEvent::focus_gained:
    case provider::LifecycleEvent::focus_lost:
      ++focus_changes_;
      break;
    case provider::LifecycleEvent::exit_requested:
    case provider::LifecycleEvent::operation_mode_changed:
      break;
  }
  if (observer_) {
    observer_(*event);
  }
  return event;
}

auto SoakLifecycle::name(provider::LifecycleEvent event) noexcept
    -> std::string_view {
  switch (event) {
    case provider::LifecycleEvent::focus_gained:
      return "focus_gained";
    case provider::LifecycleEvent::focus_lost:
      return "focus_lost";
    case provider::LifecycleEvent::suspending:
      return "suspending";
    case provider::LifecycleEvent::resumed:
      return "resumed";
    case provider::LifecycleEvent::exit_requested:
      return "exit_requested";
    case provider::LifecycleEvent::operation_mode_changed:
      return "operation_mode_changed";
  }
  return "unknown";
}

}
