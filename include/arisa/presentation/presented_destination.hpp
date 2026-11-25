#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/navigation_path.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/presented_item.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/presentation/navigation_stack.hpp>

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::presentation {

class PresentedDestination : public composition::Element {
 public:
  class Content;

  using Build = std::function<composition::Element()>;
  using Dismissed = std::function<void()>;

  static constexpr std::string_view presented_key = "arisa.destination";

  PresentedDestination(composition::Element host,
                       std::optional<composition::ElementKey> presented,
                       Build build, Dismissed dismissed);
};

class PresentedDestination::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  struct Pushed {
    std::uint64_t serial{0};
    composition::ElementKey key;
    composition::Identity view;

    friend auto operator==(const Pushed&, const Pushed&) -> bool = default;
  };

  static const composition::StateKey<std::optional<Pushed>> pushed;
  static constexpr composition::StateKey<std::uint64_t> serials{
      "arisa.destination.serial", 0};

  Content(std::optional<composition::ElementKey> presented, Build build,
          Dismissed dismissed) noexcept
      : presented_(std::move(presented)),
        build_(std::move(build)),
        dismissed_(std::move(dismissed)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  void hold(Source host) noexcept { host_ = std::move(host); }

 private:
  struct Request {
    NavigationStack::Navigator navigator;
    composition::StateHandle<std::optional<Pushed>> held;
    composition::StateHandle<std::uint64_t> counter;
    std::optional<composition::ElementKey> presented;
    Build build;
    Dismissed dismissed;
  };

  static void sync(const Request& request);

  static auto destination(const Request& request, std::uint64_t serial)
      -> NavigationStack::ViewDestination;

  std::optional<composition::ElementKey> presented_;
  Build build_;
  Dismissed dismissed_;
  Source host_;
};

}

namespace arisa::composition {

template <class Content>
auto Element::navigation_destination(Binding<bool> is_presented,
                                     Content content) && -> Element {
  const bool shown = is_presented.get();
  return presentation::PresentedDestination{
      std::move(*this),
      shown ? std::optional<ElementKey>{ElementKey{
                  presentation::PresentedDestination::presented_key}}
            : std::nullopt,
      [content = std::move(content)] { return Element{std::invoke(content)}; },
      [is_presented] { is_presented.set(false); }};
}

template <PresentedItem Item, class Build>
auto Element::navigation_destination(Item item, Build build) && -> Element {
  using Value = PresentedValue<Item>;
  const Binding<std::optional<Value>> bound{std::move(item)};
  const std::optional<Value> value = bound.get();
  std::optional<ElementKey> key;
  presentation::PresentedDestination::Build made;
  if (value.has_value()) {
    key = ElementKey::identifying(*value);
    made = [build = std::move(build), held = *value] {
      return Element{std::invoke(build, held)};
    };
  }
  return presentation::PresentedDestination{
      std::move(*this), std::move(key), std::move(made),
      [bound] { bound.set(std::nullopt); }};
}

}
