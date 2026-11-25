#pragma once

#include <arisa/composition/event_modifiers.hpp>

#include <functional>
#include <utility>

namespace arisa::composition {

class ModifierKeysHandler {
 public:
  using Action = std::move_only_function<void(EventModifiers, EventModifiers)>;

  ModifierKeysHandler(EventModifiers mask, bool initial, Action action) noexcept
      : mask_(mask), initial_(initial), action_(std::move(action)) {}

  auto mask() const noexcept -> EventModifiers { return mask_; }

  auto initial() const noexcept -> bool { return initial_; }

  auto differs(EventModifiers old, EventModifiers next) const noexcept -> bool {
    return (old & mask_) != (next & mask_);
  }

  void changed(EventModifiers old, EventModifiers next) {
    if (action_) {
      action_(old & mask_, next & mask_);
    }
  }

 private:
  EventModifiers mask_;
  bool initial_;
  Action action_;
};

}
