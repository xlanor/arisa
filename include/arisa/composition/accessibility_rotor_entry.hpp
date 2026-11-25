#pragma once

#include <arisa/composition/accessibility_text.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/namespace.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <utility>

namespace arisa::composition {

class AccessibilityRotorEntry {
 public:
  struct TextRange {
    std::size_t lower{0};
    std::size_t upper{0};

    friend constexpr auto operator==(const TextRange&, const TextRange&)
        -> bool = default;
  };

  using Prepare = std::function<void()>;

  template <Identifiable ID>
  AccessibilityRotorEntry(AccessibilityText label, const ID& id,
                          std::optional<TextRange> text_range = std::nullopt,
                          Prepare prepare = {})
      : label_(std::move(label)),
        id_(ElementKey::identifying(id)),
        text_range_(text_range),
        prepare_(std::move(prepare)) {}

  template <Identifiable ID>
  AccessibilityRotorEntry(AccessibilityText label, const ID& id,
                          Namespace::ID in,
                          std::optional<TextRange> text_range = std::nullopt,
                          Prepare prepare = {})
      : label_(std::move(label)),
        id_(ElementKey::identifying(id)),
        space_(in),
        text_range_(text_range),
        prepare_(std::move(prepare)) {}

  AccessibilityRotorEntry(std::optional<AccessibilityText> label,
                          TextRange text_range, Prepare prepare = {})
      : label_(std::move(label)),
        text_range_(text_range),
        prepare_(std::move(prepare)) {}

  auto label() const noexcept -> const std::optional<AccessibilityText>& {
    return label_;
  }

  auto id() const noexcept -> const std::optional<ElementKey>& { return id_; }

  auto space() const noexcept -> const std::optional<Namespace::ID>& {
    return space_;
  }

  auto text_range() const noexcept -> const std::optional<TextRange>& {
    return text_range_;
  }

  void prepare() const {
    if (prepare_) {
      prepare_();
    }
  }

 private:
  std::optional<AccessibilityText> label_;
  std::optional<ElementKey> id_;
  std::optional<Namespace::ID> space_;
  std::optional<TextRange> text_range_;
  Prepare prepare_;
};

}
