#pragma once

#include <arisa/composition/accessibility_text.hpp>

#include <string>
#include <utility>

namespace arisa::composition {

class AccessibilityCustomContentKey {
 public:
  AccessibilityCustomContentKey(AccessibilityText label, std::string id)
      : label_(std::move(label)), id_(std::move(id)) {}

  explicit AccessibilityCustomContentKey(AccessibilityText label)
      : label_(std::move(label)), id_(label_.verbatim()) {}

  auto label() const noexcept -> const AccessibilityText& { return label_; }

  auto id() const noexcept -> const std::string& { return id_; }

  friend auto operator==(const AccessibilityCustomContentKey& left,
                         const AccessibilityCustomContentKey& right) -> bool {
    return left.id_ == right.id_;
  }

 private:
  AccessibilityText label_;
  std::string id_;
};

}
