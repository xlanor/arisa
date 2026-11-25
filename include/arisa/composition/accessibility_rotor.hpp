#pragma once

#include <arisa/composition/accessibility_rotor_entry.hpp>
#include <arisa/composition/accessibility_system_rotor.hpp>
#include <arisa/composition/accessibility_text.hpp>

#include <functional>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

namespace arisa::composition {

class AccessibilityRotor {
 public:
  using Entries = std::function<std::vector<AccessibilityRotorEntry>()>;
  using Label = std::variant<AccessibilityText, AccessibilitySystemRotor>;

  AccessibilityRotor(Label label, Entries entries)
      : label_(std::move(label)),
        entries_(std::make_shared<const Entries>(std::move(entries))) {}

  auto label() const noexcept -> const Label& { return label_; }

  auto system() const noexcept -> const AccessibilitySystemRotor* {
    return std::get_if<AccessibilitySystemRotor>(&label_);
  }

  auto entries() const -> std::vector<AccessibilityRotorEntry> {
    return *entries_ ? (*entries_)() : std::vector<AccessibilityRotorEntry>{};
  }

  friend auto operator==(const AccessibilityRotor& left,
                         const AccessibilityRotor& right) -> bool {
    return left.label_ == right.label_ && left.entries_ == right.entries_;
  }

 private:
  Label label_;
  std::shared_ptr<const Entries> entries_;
};

}
