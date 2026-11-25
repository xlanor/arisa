#pragma once

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/presentation/presentation_layer.hpp>
#include <arisa/presentation/presentations.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::presentation {

class Alert {
 public:
  class Button {
   public:
    using Action = std::function<void()>;

    static auto default_button(std::string label, Action action = {})
        -> Button {
      return Button{std::move(label), std::nullopt, std::move(action)};
    }

    static auto cancel(Action action = {}) -> Button {
      return Button{std::string{PresentationLayer::cancel_label},
                    components::ButtonRole::cancel, std::move(action)};
    }

    static auto cancel(std::string label, Action action = {}) -> Button {
      return Button{std::move(label), components::ButtonRole::cancel,
                    std::move(action)};
    }

    static auto destructive(std::string label, Action action = {}) -> Button {
      return Button{std::move(label), components::ButtonRole::destructive,
                    std::move(action)};
    }

    auto label() const noexcept -> const std::string& { return label_; }

    auto role() const noexcept -> std::optional<components::ButtonRole> {
      return role_;
    }

    auto element() const -> composition::Element {
      return components::Button(label_, role_, [action = action_] {
        if (action) {
          action();
        }
      });
    }

   private:
    Button(std::string label, std::optional<components::ButtonRole> role,
           Action action) noexcept
        : label_(std::move(label)), role_(role), action_(std::move(action)) {}

    std::string label_;
    std::optional<components::ButtonRole> role_;
    Action action_;
  };

  explicit Alert(std::string title, std::optional<std::string> message = {},
                 std::optional<Button> dismiss_button = {})
      : title_(std::move(title)), message_(std::move(message)) {
    buttons_.push_back(
        dismiss_button.has_value()
            ? std::move(*dismiss_button)
            : Button::default_button(std::string{PresentationLayer::ok_label}));
  }

  Alert(std::string title, std::optional<std::string> message,
        Button primary_button, Button secondary_button)
      : title_(std::move(title)), message_(std::move(message)) {
    buttons_.push_back(std::move(primary_button));
    buttons_.push_back(std::move(secondary_button));
  }

  auto title() const noexcept -> const std::string& { return title_; }

  auto message() const noexcept -> const std::optional<std::string>& {
    return message_;
  }

  auto buttons() const noexcept -> const std::vector<Button>& {
    return buttons_;
  }

  auto actions() const -> composition::Element {
    std::vector<composition::Element> made;
    made.reserve(buttons_.size());
    for (const Button& each : buttons_) {
      made.push_back(each.element());
    }
    return components::Group(std::move(made));
  }

 private:
  std::string title_;
  std::optional<std::string> message_;
  std::vector<Button> buttons_;
};

class ActionSheet {
 public:
  ActionSheet(std::string title, std::optional<std::string> message = {},
              std::vector<Alert::Button> buttons = {Alert::Button::cancel()})
      : title_(std::move(title)),
        message_(std::move(message)),
        buttons_(std::move(buttons)) {}

  auto title() const noexcept -> const std::string& { return title_; }

  auto message() const noexcept -> const std::optional<std::string>& {
    return message_;
  }

  auto actions() const -> composition::Element {
    std::vector<composition::Element> made;
    made.reserve(buttons_.size());
    for (const Alert::Button& each : buttons_) {
      made.push_back(each.element());
    }
    return components::Group(std::move(made));
  }

 private:
  std::string title_;
  std::optional<std::string> message_;
  std::vector<Alert::Button> buttons_;
};

}

namespace arisa::composition {

template <class Build>
  requires std::invocable<const Build&>
auto Element::alert(Binding<bool> is_presented, Build content) && -> Element {
  std::string title;
  std::optional<Element> declared;
  std::optional<Element> said;
  if (is_presented.get()) {
    const presentation::Alert built{std::invoke(content)};
    title = built.title();
    declared = built.actions();
    if (built.message().has_value()) {
      said = components::Text(*built.message());
    }
  }
  return presentation::PresentationLayer::alert(
      std::move(*this), presentation::Presentations::presence(is_presented),
      std::move(title), std::move(declared), std::move(said));
}

template <class Build>
  requires std::invocable<const Build&>
auto Element::action_sheet(Binding<bool> is_presented,
                           Build content) && -> Element {
  std::string title;
  std::optional<Element> declared;
  std::optional<Element> said;
  if (is_presented.get()) {
    const presentation::ActionSheet built{std::invoke(content)};
    title = built.title();
    declared = built.actions();
    if (built.message().has_value()) {
      said = components::Text(*built.message());
    }
  }
  return presentation::PresentationLayer::confirmation_dialog(
      std::move(*this), presentation::Presentations::presence(is_presented),
      std::move(title), Visibility::visible, std::move(declared),
      std::move(said));
}

}
