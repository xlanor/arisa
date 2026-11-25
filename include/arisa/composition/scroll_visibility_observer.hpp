#pragma once

#include <functional>
#include <utility>

namespace arisa::composition {

class ScrollVisibilityObserver {
 public:
  using Action = std::move_only_function<void(bool)>;

  ScrollVisibilityObserver(double threshold, Action action) noexcept
      : threshold_(threshold), action_(std::move(action)) {}

  auto threshold() const noexcept -> double { return threshold_; }

  void notify(bool visible) {
    if (action_) {
      action_(visible);
    }
  }

 private:
  double threshold_;
  Action action_;
};

}
