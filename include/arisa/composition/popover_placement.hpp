#pragma once

#include <arisa/theme/space.hpp>

#include <functional>
#include <memory>
#include <utility>

namespace arisa::composition {

class PopoverPlacement {
 public:
  using Action = std::function<void(theme::Edge)>;

  PopoverPlacement(theme::Edge composed, Action action)
      : composed_(composed),
        action_(std::make_shared<const Action>(std::move(action))) {}

  auto composed() const noexcept -> theme::Edge { return composed_; }

  void place(theme::Edge edge) const {
    if (edge != composed_ && *action_) {
      (*action_)(edge);
    }
  }

 private:
  theme::Edge composed_;
  std::shared_ptr<const Action> action_;
};

}
