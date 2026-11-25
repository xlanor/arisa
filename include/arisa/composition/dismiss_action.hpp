#pragma once

#include <arisa/composition/identity.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <utility>

namespace arisa::composition {

class DismissAction {
 public:
  DismissAction() = default;

  DismissAction(Identity source, std::size_t level,
                std::function<void()> action)
      : source_(source), level_(level), action_(std::move(action)) {}

  void operator()() const {
    if (action_) {
      action_();
    }
  }

  auto available() const noexcept -> bool { return static_cast<bool>(action_); }

  friend auto operator==(const DismissAction& left, const DismissAction& right)
      -> bool {
    return left.source_ == right.source_ && left.level_ == right.level_;
  }

 private:
  std::optional<Identity> source_;
  std::size_t level_{0};
  std::function<void()> action_;
};

}
