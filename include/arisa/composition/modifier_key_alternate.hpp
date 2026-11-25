#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/event_modifiers.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_key.hpp>

#include <memory>
#include <vector>

namespace arisa::composition {

class ModifierKeyAlternate : public Element {
 public:
  class Content;

  ModifierKeyAlternate(EventModifiers modifiers, Element primary,
                       Element alternate);
};

class ModifierKeyAlternate::Content final : public ElementContent {
 public:
  using Holder = std::shared_ptr<PlaceholderContentView::Source>;

  static constexpr StateKey<bool> held{"modifier-key-alternate", false};

  Content(EventModifiers modifiers, Holder primary, Holder alternate) noexcept
      : modifiers_(modifiers),
        primary_(std::move(primary)),
        alternate_(std::move(alternate)) {}

  auto type() const noexcept -> ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(Context& context) const -> std::vector<Element> override;

 private:
  EventModifiers modifiers_;
  Holder primary_;
  Holder alternate_;
};

}
