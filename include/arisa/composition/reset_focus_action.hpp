#pragma once

#include <arisa/composition/focus_resets.hpp>
#include <arisa/composition/namespace.hpp>

#include <functional>
#include <memory>
#include <utility>

namespace arisa::composition {

class ResetFocusAction {
 public:
  ResetFocusAction() = default;

  ResetFocusAction(std::shared_ptr<FocusResets> resets,
                   std::function<void()> wake) noexcept
      : resets_(std::move(resets)), wake_(std::move(wake)) {}

  void operator()(Namespace::ID scope) const {
    if (resets_ == nullptr) {
      return;
    }
    resets_->push(scope);
    if (wake_) {
      wake_();
    }
  }

  friend auto operator==(const ResetFocusAction& left,
                         const ResetFocusAction& right) -> bool {
    return left.resets_ == right.resets_;
  }

 private:
  std::shared_ptr<FocusResets> resets_;
  std::function<void()> wake_;
};

}
