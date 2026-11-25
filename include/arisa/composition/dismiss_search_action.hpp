#pragma once

#include <arisa/composition/identity.hpp>

#include <functional>
#include <optional>
#include <utility>

namespace arisa::composition {

class DismissSearchAction {
 public:
  DismissSearchAction() = default;

  DismissSearchAction(Identity source, std::function<void()> action)
      : source_(source), action_(std::move(action)) {}

  void operator()() const {
    if (action_) {
      action_();
    }
  }

  auto available() const noexcept -> bool { return static_cast<bool>(action_); }

  friend auto operator==(const DismissSearchAction& left,
                         const DismissSearchAction& right) -> bool {
    return left.source_ == right.source_;
  }

 private:
  std::optional<Identity> source_;
  std::function<void()> action_;
};

}
