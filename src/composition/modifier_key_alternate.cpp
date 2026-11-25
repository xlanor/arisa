#include <arisa/composition/context.hpp>
#include <arisa/composition/modifier_key_alternate.hpp>

#include <utility>

namespace arisa::composition {

ModifierKeyAlternate::ModifierKeyAlternate(EventModifiers modifiers,
                                           Element primary, Element alternate)
    : Element(std::make_unique<Content>(
          modifiers,
          std::make_shared<PlaceholderContentView::Source>(std::move(primary)),
          std::make_shared<PlaceholderContentView::Source>(
              std::move(alternate)))) {}

auto ModifierKeyAlternate::Content::type() const noexcept -> ElementType {
  return ElementType::of<Content>();
}

auto ModifierKeyAlternate::Content::body(Context& context) const
    -> std::vector<Element> {
  const auto holding = context.state(held);
  const EventModifiers wanted = modifiers_;
  const bool shown = holding.get();
  Element alternate = Element{PlaceholderContentView(alternate_)}
                          .opacity(shown ? 1.0F : 0.0F)
                          .allows_hit_testing(shown);
  std::vector<Element> produced;
  produced.push_back(
      Element{PlaceholderContentView(primary_)}
          .opacity(shown ? 0.0F : 1.0F)
          .allows_hit_testing(!shown)
          .overlay(std::move(alternate))
          .on_modifier_keys_changed(
              wanted, true,
              [holding, wanted](EventModifiers, EventModifiers next) {
                const bool now_held = !wanted.empty() && next.contains(wanted);
                if (holding.get() != now_held) {
                  holding.set(now_held);
                }
              }));
  return produced;
}

}
