#pragma once

#include <arisa/composition/accessibility_text.hpp>

#include <cstdint>
#include <optional>
#include <utility>

namespace arisa::composition {

class AccessibilityActionKind {
 public:
  enum class Kind : std::uint8_t {
    standard,
    escape,
    magic_tap,
    remove,
    show_menu,
    named,
  };

  static const AccessibilityActionKind default_;
  static const AccessibilityActionKind escape;
  static const AccessibilityActionKind magic_tap;
  static const AccessibilityActionKind delete_;
  static const AccessibilityActionKind show_menu;

  explicit AccessibilityActionKind(AccessibilityText named)
      : kind_(Kind::named), name_(std::move(named)) {}

  auto kind() const noexcept -> Kind { return kind_; }

  auto name() const noexcept -> const std::optional<AccessibilityText>& {
    return name_;
  }

  friend auto operator==(const AccessibilityActionKind&,
                         const AccessibilityActionKind&) -> bool = default;

 private:
  explicit AccessibilityActionKind(Kind kind) noexcept : kind_(kind) {}

  Kind kind_;
  std::optional<AccessibilityText> name_;
};

inline const AccessibilityActionKind AccessibilityActionKind::default_{
    Kind::standard};
inline const AccessibilityActionKind AccessibilityActionKind::escape{
    Kind::escape};
inline const AccessibilityActionKind AccessibilityActionKind::magic_tap{
    Kind::magic_tap};
inline const AccessibilityActionKind AccessibilityActionKind::delete_{
    Kind::remove};
inline const AccessibilityActionKind AccessibilityActionKind::show_menu{
    Kind::show_menu};

}
