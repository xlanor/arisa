#pragma once

#include <arisa/composition/accessibility_text.hpp>

#include <cstdint>
#include <optional>
#include <utility>

namespace arisa::composition {

class AccessibilityActionCategory {
 public:
  static const AccessibilityActionCategory default_;
  static const AccessibilityActionCategory edit;

  explicit AccessibilityActionCategory(AccessibilityText name)
      : name_(std::move(name)) {}

  auto name() const noexcept -> const std::optional<AccessibilityText>& {
    return name_;
  }

  auto is_edit() const noexcept -> bool { return edit_; }

  friend auto operator==(const AccessibilityActionCategory&,
                         const AccessibilityActionCategory&) -> bool = default;

 private:
  enum class Builtin : std::uint8_t {
    standard,
    edit,
  };

  explicit AccessibilityActionCategory(Builtin builtin) noexcept
      : edit_(builtin == Builtin::edit) {}

  std::optional<AccessibilityText> name_;
  bool edit_{false};
};

inline const AccessibilityActionCategory AccessibilityActionCategory::default_{
    Builtin::standard};
inline const AccessibilityActionCategory AccessibilityActionCategory::edit{
    Builtin::edit};

}
