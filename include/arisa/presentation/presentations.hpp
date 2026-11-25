#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/localized_error.hpp>
#include <arisa/composition/popover_attachment_anchor.hpp>
#include <arisa/composition/presented_item.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/presentation/presentation_layer.hpp>

#include <chrono>
#include <concepts>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace arisa::presentation {

class Presentations {
 public:
  static auto presence(const composition::Binding<bool>& is_presented)
      -> PresentationLayer::Presence {
    return PresentationLayer::Presence{.presented = is_presented.get(),
                                       .dismiss =
                                           [is_presented] {
                                             if (is_presented.get()) {
                                               is_presented.set(false);
                                             }
                                           },
                                       .source = is_presented.identity(),
                                       .identity = std::nullopt};
  }

  template <class T>
  static auto presence(const composition::Binding<std::optional<T>>& item)
      -> PresentationLayer::Presence {
    return PresentationLayer::Presence{.presented = item.get().has_value(),
                                       .dismiss =
                                           [item] {
                                             if (item.get().has_value()) {
                                               item.set(std::nullopt);
                                             }
                                           },
                                       .source = item.identity(),
                                       .identity = std::nullopt};
  }

  template <composition::OptionalBindingSource Item>
  static auto bound(Item item)
      -> composition::Binding<composition::BoundValue<Item>> {
    return composition::Binding<composition::BoundValue<Item>>{std::move(item)};
  }

  template <class T>
  static auto identified(const composition::Binding<std::optional<T>>& item)
      -> PresentationLayer::Presence {
    PresentationLayer::Presence found = presence(item);
    if (const std::optional<T> value = item.get(); value.has_value()) {
      found.identity = composition::ElementKey::identifying(*value);
    }
    return found;
  }

  template <class Build>
  static auto build(bool presented, Build& content)
      -> std::optional<composition::Element> {
    if (!presented) {
      return std::nullopt;
    }
    return composition::Element{std::invoke(content)};
  }

  template <class T, class Build>
  static auto build(const std::optional<T>& item, Build& content)
      -> std::optional<composition::Element> {
    if (!item.has_value()) {
      return std::nullopt;
    }
    if constexpr (std::invocable<Build&, const T&>) {
      return composition::Element{std::invoke(content, *item)};
    } else {
      return composition::Element{std::invoke(content)};
    }
  }

  template <class Error>
  static auto described(const Error& error) -> std::string {
    return std::string{error.error_description()};
  }

  template <class Error>
  static auto titled(const Error& error) -> std::string {
    if constexpr (composition::LocalizedError<Error>) {
      return described(error);
    } else if constexpr (requires { std::string{error.description()}; }) {
      return std::string{error.description()};
    } else {
      return std::string{"Error"};
    }
  }

  template <class Error>
  static auto suggested_for(const Error& error)
      -> std::optional<composition::Element> {
    if constexpr (composition::LocalizedError<Error>) {
      return suggested(error);
    } else {
      return std::nullopt;
    }
  }

  template <class Error>
  static auto suggested(const Error& error)
      -> std::optional<composition::Element> {
    if constexpr (composition::SuggestsRecovery<Error>) {
      std::string suggestion{error.recovery_suggestion()};
      if (!suggestion.empty()) {
        return PresentationLayer::message(std::move(suggestion));
      }
    }
    return std::nullopt;
  }
};

}

namespace arisa::composition {

template <class Content>
auto Element::sheet(Binding<bool> is_presented, Content content) && -> Element {
  return std::move(*this).sheet(std::move(is_presented),
                                presentation::PresentationLayer::Dismissed{},
                                std::move(content));
}

template <class OnDismiss, class Content>
auto Element::sheet(Binding<bool> is_presented, OnDismiss on_dismiss,
                    Content content) && -> Element {
  auto shown = presentation::Presentations::build(is_presented.get(), content);
  return presentation::PresentationLayer::sheet(
      std::move(*this), presentation::Presentations::presence(is_presented),
      presentation::PresentationLayer::Dismissed{std::move(on_dismiss)},
      std::move(shown));
}

template <PresentedItem Item, class Content>
auto Element::sheet(Item item, Content content) && -> Element {
  return std::move(*this).sheet(std::move(item),
                                presentation::PresentationLayer::Dismissed{},
                                std::move(content));
}

template <PresentedItem Item, class OnDismiss, class Content>
auto Element::sheet(Item item, OnDismiss on_dismiss,
                    Content content) && -> Element {
  const auto bound = presentation::Presentations::bound(std::move(item));
  auto shown = presentation::Presentations::build(bound.get(), content);
  return presentation::PresentationLayer::sheet(
      std::move(*this), presentation::Presentations::identified(bound),
      presentation::PresentationLayer::Dismissed{std::move(on_dismiss)},
      std::move(shown));
}

template <class Content>
auto Element::full_screen_cover(Binding<bool> is_presented,
                                Content content) && -> Element {
  return std::move(*this).full_screen_cover(
      std::move(is_presented), presentation::PresentationLayer::Dismissed{},
      std::move(content));
}

template <class OnDismiss, class Content>
auto Element::full_screen_cover(Binding<bool> is_presented,
                                OnDismiss on_dismiss,
                                Content content) && -> Element {
  auto shown = presentation::Presentations::build(is_presented.get(), content);
  return presentation::PresentationLayer::full_screen_cover(
      std::move(*this), presentation::Presentations::presence(is_presented),
      presentation::PresentationLayer::Dismissed{std::move(on_dismiss)},
      std::move(shown));
}

template <PresentedItem Item, class Content>
auto Element::full_screen_cover(Item item, Content content) && -> Element {
  return std::move(*this).full_screen_cover(
      std::move(item), presentation::PresentationLayer::Dismissed{},
      std::move(content));
}

template <PresentedItem Item, class OnDismiss, class Content>
auto Element::full_screen_cover(Item item, OnDismiss on_dismiss,
                                Content content) && -> Element {
  const auto bound = presentation::Presentations::bound(std::move(item));
  auto shown = presentation::Presentations::build(bound.get(), content);
  return presentation::PresentationLayer::full_screen_cover(
      std::move(*this), presentation::Presentations::identified(bound),
      presentation::PresentationLayer::Dismissed{std::move(on_dismiss)},
      std::move(shown));
}

template <class Actions>
auto Element::alert(std::string title, Binding<bool> is_presented,
                    Actions actions) && -> Element {
  auto declared =
      presentation::Presentations::build(is_presented.get(), actions);
  return presentation::PresentationLayer::alert(
      std::move(*this), presentation::Presentations::presence(is_presented),
      std::move(title), std::move(declared), std::nullopt);
}

template <class Actions, class Message>
auto Element::alert(std::string title, Binding<bool> is_presented,
                    Actions actions, Message message) && -> Element {
  auto declared =
      presentation::Presentations::build(is_presented.get(), actions);
  auto said = presentation::Presentations::build(is_presented.get(), message);
  return presentation::PresentationLayer::alert(
      std::move(*this), presentation::Presentations::presence(is_presented),
      std::move(title), std::move(declared), std::move(said));
}

template <class T, class Actions>
auto Element::alert(std::string title, Binding<bool> is_presented,
                    std::optional<T> presenting,
                    Actions actions) && -> Element {
  const std::optional<T> shown =
      is_presented.get() ? std::move(presenting) : std::nullopt;
  auto declared = presentation::Presentations::build(shown, actions);
  presentation::PresentationLayer::Presence presence =
      presentation::Presentations::presence(is_presented);
  presence.presented = shown.has_value();
  return presentation::PresentationLayer::alert(
      std::move(*this), std::move(presence), std::move(title),
      std::move(declared), std::nullopt);
}

template <class T, class Actions, class Message>
auto Element::alert(std::string title, Binding<bool> is_presented,
                    std::optional<T> presenting, Actions actions,
                    Message message) && -> Element {
  const std::optional<T> shown =
      is_presented.get() ? std::move(presenting) : std::nullopt;
  auto declared = presentation::Presentations::build(shown, actions);
  auto said = presentation::Presentations::build(shown, message);
  presentation::PresentationLayer::Presence presence =
      presentation::Presentations::presence(is_presented);
  presence.presented = shown.has_value();
  return presentation::PresentationLayer::alert(
      std::move(*this), std::move(presence), std::move(title),
      std::move(declared), std::move(said));
}

template <OptionalBindingSource Item, class Actions>
auto Element::alert(std::string title, Item item,
                    Actions actions) && -> Element {
  const auto bound = presentation::Presentations::bound(std::move(item));
  auto declared = presentation::Presentations::build(bound.get(), actions);
  return presentation::PresentationLayer::alert(
      std::move(*this), presentation::Presentations::presence(bound),
      std::move(title), std::move(declared), std::nullopt);
}

template <OptionalBindingSource Item, class Actions, class Message>
auto Element::alert(std::string title, Item item, Actions actions,
                    Message message) && -> Element {
  const auto bound = presentation::Presentations::bound(std::move(item));
  const auto value = bound.get();
  auto declared = presentation::Presentations::build(value, actions);
  auto said = presentation::Presentations::build(value, message);
  return presentation::PresentationLayer::alert(
      std::move(*this), presentation::Presentations::presence(bound),
      std::move(title), std::move(declared), std::move(said));
}

template <LocalizedError E, class Actions>
auto Element::alert(Binding<bool> is_presented, std::optional<E> error,
                    Actions actions) && -> Element {
  const bool shown = is_presented.get() && error.has_value();
  std::string title = error.has_value()
                          ? presentation::Presentations::described(*error)
                          : std::string{};
  std::optional<Element> said;
  if (shown) {
    said = presentation::Presentations::suggested(*error);
  }
  auto declared = presentation::Presentations::build(
      shown ? std::move(error) : std::nullopt, actions);
  presentation::PresentationLayer::Presence presence =
      presentation::Presentations::presence(is_presented);
  presence.presented = shown;
  return presentation::PresentationLayer::alert(
      std::move(*this), std::move(presence), std::move(title),
      std::move(declared), std::move(said));
}

template <LocalizedError E, class Actions, class Message>
auto Element::alert(Binding<bool> is_presented, std::optional<E> error,
                    Actions actions, Message message) && -> Element {
  const bool shown = is_presented.get() && error.has_value();
  std::string title = error.has_value()
                          ? presentation::Presentations::described(*error)
                          : std::string{};
  const std::optional<E> presented = shown ? std::move(error) : std::nullopt;
  auto declared = presentation::Presentations::build(presented, actions);
  auto said = presentation::Presentations::build(presented, message);
  presentation::PresentationLayer::Presence presence =
      presentation::Presentations::presence(is_presented);
  presence.presented = shown;
  return presentation::PresentationLayer::alert(
      std::move(*this), std::move(presence), std::move(title),
      std::move(declared), std::move(said));
}

template <class T, LocalizedError E, class Actions>
auto Element::alert(Binding<bool> is_presented,
                    const std::expected<T, E>& result,
                    Actions actions) && -> Element {
  std::optional<E> error;
  if (!result.has_value()) {
    error = result.error();
  }
  return std::move(*this).alert(std::move(is_presented), std::move(error),
                                std::move(actions));
}

template <class Actions>
auto Element::confirmation_dialog(std::string title, Binding<bool> is_presented,
                                  Visibility title_visibility,
                                  Actions actions) && -> Element {
  auto declared =
      presentation::Presentations::build(is_presented.get(), actions);
  return presentation::PresentationLayer::confirmation_dialog(
      std::move(*this), presentation::Presentations::presence(is_presented),
      std::move(title), title_visibility, std::move(declared), std::nullopt);
}

template <class Actions, class Message>
auto Element::confirmation_dialog(std::string title, Binding<bool> is_presented,
                                  Visibility title_visibility, Actions actions,
                                  Message message) && -> Element {
  auto declared =
      presentation::Presentations::build(is_presented.get(), actions);
  auto said = presentation::Presentations::build(is_presented.get(), message);
  return presentation::PresentationLayer::confirmation_dialog(
      std::move(*this), presentation::Presentations::presence(is_presented),
      std::move(title), title_visibility, std::move(declared), std::move(said));
}

template <class T, class Actions>
auto Element::confirmation_dialog(std::string title, Binding<bool> is_presented,
                                  Visibility title_visibility,
                                  std::optional<T> presenting,
                                  Actions actions) && -> Element {
  const std::optional<T> shown =
      is_presented.get() ? std::move(presenting) : std::nullopt;
  auto declared = presentation::Presentations::build(shown, actions);
  presentation::PresentationLayer::Presence presence =
      presentation::Presentations::presence(is_presented);
  presence.presented = shown.has_value();
  return presentation::PresentationLayer::confirmation_dialog(
      std::move(*this), std::move(presence), std::move(title), title_visibility,
      std::move(declared), std::nullopt);
}

template <class T, class Actions, class Message>
auto Element::confirmation_dialog(std::string title, Binding<bool> is_presented,
                                  Visibility title_visibility,
                                  std::optional<T> presenting, Actions actions,
                                  Message message) && -> Element {
  const std::optional<T> shown =
      is_presented.get() ? std::move(presenting) : std::nullopt;
  auto declared = presentation::Presentations::build(shown, actions);
  auto said = presentation::Presentations::build(shown, message);
  presentation::PresentationLayer::Presence presence =
      presentation::Presentations::presence(is_presented);
  presence.presented = shown.has_value();
  return presentation::PresentationLayer::confirmation_dialog(
      std::move(*this), std::move(presence), std::move(title), title_visibility,
      std::move(declared), std::move(said));
}

template <OptionalBindingSource Item, class Actions>
auto Element::confirmation_dialog(std::string title, Item item,
                                  Visibility title_visibility,
                                  Actions actions) && -> Element {
  const auto bound = presentation::Presentations::bound(std::move(item));
  auto declared = presentation::Presentations::build(bound.get(), actions);
  return presentation::PresentationLayer::confirmation_dialog(
      std::move(*this), presentation::Presentations::presence(bound),
      std::move(title), title_visibility, std::move(declared), std::nullopt);
}

template <OptionalBindingSource Item, class Actions, class Message>
auto Element::confirmation_dialog(std::string title, Item item,
                                  Visibility title_visibility, Actions actions,
                                  Message message) && -> Element {
  const auto bound = presentation::Presentations::bound(std::move(item));
  const auto value = bound.get();
  auto declared = presentation::Presentations::build(value, actions);
  auto said = presentation::Presentations::build(value, message);
  return presentation::PresentationLayer::confirmation_dialog(
      std::move(*this), presentation::Presentations::presence(bound),
      std::move(title), title_visibility, std::move(declared), std::move(said));
}

template <OptionalBindingSource Item, class Actions>
auto Element::alert(Item error_source, Actions actions) && -> Element {
  using E = typename BoundValue<Item>::value_type;
  const auto error =
      presentation::Presentations::bound(std::move(error_source));
  const std::optional<E> shown = error.get();
  std::string title = shown.has_value()
                          ? presentation::Presentations::titled(*shown)
                          : std::string{};
  std::optional<Element> said;
  if (shown.has_value()) {
    said = presentation::Presentations::suggested_for(*shown);
  }
  std::optional<Element> declared;
  if (shown.has_value()) {
    if constexpr (std::invocable<Actions&, const E&>) {
      declared = Element{std::invoke(actions, *shown)};
    } else {
      declared = Element{std::invoke(actions)};
    }
  }
  return presentation::PresentationLayer::alert(
      std::move(*this), presentation::Presentations::presence(error),
      std::move(title), std::move(declared), std::move(said));
}

template <OptionalBindingSource Item, class Actions, class Message>
auto Element::alert(Item error_source, Actions actions,
                    Message message) && -> Element {
  using E = typename BoundValue<Item>::value_type;
  const auto error =
      presentation::Presentations::bound(std::move(error_source));
  const std::optional<E> shown = error.get();
  std::string title = shown.has_value()
                          ? presentation::Presentations::titled(*shown)
                          : std::string{};
  auto declared = presentation::Presentations::build(shown, actions);
  auto said = presentation::Presentations::build(shown, message);
  return presentation::PresentationLayer::alert(
      std::move(*this), presentation::Presentations::presence(error),
      std::move(title), std::move(declared), std::move(said));
}

template <class Actions>
auto Element::dismissal_confirmation_dialog(std::string title,
                                            bool should_present,
                                            Actions actions) && -> Element {
  edit_presentation().dismissal_confirmation.emplace(
      PresentationOptions::Confirmation{
          .title = std::move(title),
          .should_present = should_present,
          .actions = [held = std::make_shared<Actions>(std::move(
                          actions))] { return Element{std::invoke(*held)}; },
          .message = {}});
  return std::move(*this);
}

template <class Actions, class Message>
auto Element::dismissal_confirmation_dialog(std::string title,
                                            bool should_present,
                                            Actions actions,
                                            Message message) && -> Element {
  edit_presentation().dismissal_confirmation.emplace(
      PresentationOptions::Confirmation{
          .title = std::move(title),
          .should_present = should_present,
          .actions = [held = std::make_shared<Actions>(std::move(
                          actions))] { return Element{std::invoke(*held)}; },
          .message = [held = std::make_shared<Message>(std::move(
                          message))] { return Element{std::invoke(*held)}; }});
  return std::move(*this);
}

template <class Content>
auto Element::popover(Binding<bool> is_presented,
                      Content content) && -> Element {
  return std::move(*this).popover(std::move(is_presented),
                                  PopoverAttachmentAnchor::rect(),
                                  theme::Edge::top, std::move(content));
}

template <class Content>
auto Element::popover(Binding<bool> is_presented,
                      PopoverAttachmentAnchor attachment_anchor,
                      theme::Edge arrow_edge, Content content) && -> Element {
  auto shown = presentation::Presentations::build(is_presented.get(), content);
  return presentation::PresentationLayer::popover(
      std::move(*this), presentation::Presentations::presence(is_presented),
      attachment_anchor, arrow_edge, std::move(shown));
}

template <PresentedItem Item, class Content>
auto Element::popover(Item item, Content content) && -> Element {
  return std::move(*this).popover(std::move(item),
                                  PopoverAttachmentAnchor::rect(),
                                  theme::Edge::top, std::move(content));
}

template <PresentedItem Item, class Content>
auto Element::popover(Item item, PopoverAttachmentAnchor attachment_anchor,
                      theme::Edge arrow_edge, Content content) && -> Element {
  const auto bound = presentation::Presentations::bound(std::move(item));
  auto shown = presentation::Presentations::build(bound.get(), content);
  return presentation::PresentationLayer::popover(
      std::move(*this), presentation::Presentations::identified(bound),
      attachment_anchor, arrow_edge, std::move(shown));
}

template <class Content>
auto Element::toast(Binding<bool> is_presented,
                    std::chrono::nanoseconds duration,
                    Content content) && -> Element {
  return std::move(*this).toast(std::move(is_presented), duration,
                                ToastOptions{}, std::move(content));
}

template <OptionalBindingSource Item, class Content>
auto Element::toast(Item item, std::chrono::nanoseconds duration,
                    Content content) && -> Element {
  return std::move(*this).toast(std::move(item), duration, ToastOptions{},
                                std::move(content));
}

template <class Content>
auto Element::toast(Binding<bool> is_presented,
                    std::chrono::nanoseconds duration, ToastOptions options,
                    Content content) && -> Element {
  auto shown = presentation::Presentations::build(is_presented.get(), content);
  return presentation::PresentationLayer::toast(
      std::move(*this), presentation::Presentations::presence(is_presented),
      duration, options, std::move(shown));
}

template <OptionalBindingSource Item, class Content>
auto Element::toast(Item item, std::chrono::nanoseconds duration,
                    ToastOptions options, Content content) && -> Element {
  const auto bound = presentation::Presentations::bound(std::move(item));
  auto shown = presentation::Presentations::build(bound.get(), content);
  return presentation::PresentationLayer::toast(
      std::move(*this), presentation::Presentations::presence(bound), duration,
      options, std::move(shown));
}

}
