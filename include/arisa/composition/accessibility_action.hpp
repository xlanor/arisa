#pragma once

#include <arisa/composition/accessibility_action_category.hpp>
#include <arisa/composition/accessibility_action_kind.hpp>

#include <functional>
#include <memory>
#include <utility>

namespace arisa::composition {

class AccessibilityAction {
 public:
  using Handler = std::function<void()>;

  AccessibilityAction(AccessibilityActionKind kind, Handler handler,
                      AccessibilityActionCategory category =
                          AccessibilityActionCategory::default_)
      : kind_(std::move(kind)),
        category_(std::move(category)),
        handler_(std::make_shared<const Handler>(std::move(handler))) {}

  auto kind() const noexcept -> const AccessibilityActionKind& { return kind_; }

  auto category() const noexcept -> const AccessibilityActionCategory& {
    return category_;
  }

  void perform() const {
    if (*handler_) {
      (*handler_)();
    }
  }

  friend auto operator==(const AccessibilityAction& left,
                         const AccessibilityAction& right) -> bool {
    return left.kind_ == right.kind_ && left.category_ == right.category_ &&
           left.handler_ == right.handler_;
  }

 private:
  AccessibilityActionKind kind_;
  AccessibilityActionCategory category_;
  std::shared_ptr<const Handler> handler_;
};

}
