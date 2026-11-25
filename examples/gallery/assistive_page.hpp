#pragma once

#include <arisa/composition/accessibility_focus_state.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <string_view>

namespace arisa::examples {

class AssistivePage final : public composition::Component {
 public:
  class Opener;
  class Demos;

  static constexpr std::string_view close_button = "assistive-close";

  explicit AssistivePage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const AssistivePage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  composition::StateHandle<bool> shown_;
};

class AssistivePage::Demos final : public composition::Component {
 public:
  static constexpr composition::StateKey<int> archived_key{"assistive-archived",
                                                           0};
  static constexpr composition::StateKey<int> rating_key{"assistive-rating", 3};
  static constexpr composition::StateKey<double> volume_key{"assistive-volume",
                                                            0.5};
  static constexpr composition::AccessibilityFocusState<bool> summary_focus{
      "assistive-summary"};
  static constexpr composition::Namespace rotor_space{"assistive-rotor"};
  static constexpr std::string_view mail_row = "assistive-mail";
  static constexpr std::string_view rating_row = "assistive-rating";
  static constexpr std::string_view volume_row = "assistive-volume";
  static constexpr std::string_view summary_button = "assistive-summary";
  static constexpr std::string_view jump_button = "assistive-jump";
  static constexpr std::string_view swatch = "assistive-swatch";

  auto operator==(const Demos&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto card(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto settings(composition::Context& context) -> composition::Element;

  static auto actions(composition::StateHandle<int> archived)
      -> composition::Element;

  static auto adjustable(composition::StateHandle<int> rating,
                         composition::StateHandle<double> volume)
      -> composition::Element;

  static auto rotors() -> composition::Element;

  static auto focus(const composition::AccessibilityFocusBinding<bool>& summary)
      -> composition::Element;

  static auto appearance() -> composition::Element;
};

class AssistivePage::Opener final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"assistive-shown",
                                                         false};
  static constexpr std::string_view button = "assistive-open";

  auto operator==(const Opener&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
