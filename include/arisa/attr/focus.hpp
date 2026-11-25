#pragma once

#include <arisa/composition/command.hpp>
#include <arisa/composition/command_options.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/focus_binding.hpp>
#include <arisa/composition/focus_scope_behavior.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/sensory_feedback.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <concepts>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

namespace arisa::attr {

class Focusable {
 public:
  constexpr explicit Focusable(bool is_focusable = true) noexcept
      : focusable_(is_focusable) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).focusable(focusable_);
  }

 private:
  bool focusable_;
};

class FocusEffectDisabled {
 public:
  constexpr explicit FocusEffectDisabled(bool disabled = true) noexcept
      : disabled_(disabled) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).focus_effect_disabled(disabled_);
  }

 private:
  bool disabled_;
};

class SensoryFeedbackDisabled {
 public:
  constexpr explicit SensoryFeedbackDisabled(bool disabled = true) noexcept
      : disabled_(disabled) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).sensory_feedback_disabled(disabled_);
  }

 private:
  bool disabled_;
};

class Deferred {
 public:
  using Write =
      std::move_only_function<composition::Element(composition::Element)>;

  explicit Deferred(Write write) noexcept : write_(std::move(write)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return write_(std::move(element));
  }

 private:
  Write write_;
};

class Focused : public Deferred {
 public:
  explicit Focused(composition::FocusBinding<bool> condition)
      : Deferred([condition = std::move(condition)](
                     composition::Element element) mutable {
          return std::move(element).focused(std::move(condition));
        }) {}

  template <std::equality_comparable T>
    requires(!std::same_as<T, bool>)
  Focused(composition::FocusBinding<T> binding, std::type_identity_t<T> equals)
      : Deferred([binding = std::move(binding), equals = std::move(equals)](
                     composition::Element element) mutable {
          return std::move(element).focused(std::move(binding),
                                            std::move(equals));
        }) {}
};

class DefaultFocus : public Deferred {
 public:
  explicit DefaultFocus(composition::FocusBinding<bool> binding,
                        bool value = true)
      : Deferred([binding = std::move(binding),
                  value](composition::Element element) mutable {
          return std::move(element).default_focus(std::move(binding), value);
        }) {}

  template <std::equality_comparable T>
    requires(!std::same_as<T, bool>)
  DefaultFocus(composition::FocusBinding<T> binding,
               std::type_identity_t<T> value)
      : Deferred([binding = std::move(binding), value = std::move(value)](
                     composition::Element element) mutable {
          return std::move(element).default_focus(std::move(binding),
                                                  std::move(value));
        }) {}
};

class PrefersDefaultFocus {
 public:
  explicit PrefersDefaultFocus(composition::Namespace::ID in) noexcept
      : prefers_(true), in_(in) {}

  PrefersDefaultFocus(bool prefers, composition::Namespace::ID in) noexcept
      : prefers_(prefers), in_(in) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).prefers_default_focus(prefers_, in_);
  }

 private:
  bool prefers_;
  composition::Namespace::ID in_;
};

class FocusScope {
 public:
  explicit FocusScope(composition::Namespace::ID scope,
                      composition::FocusScopeBehavior behavior =
                          composition::FocusScopeBehavior::open) noexcept
      : id_(scope), behavior_(behavior) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).focus_scope(id_, behavior_);
  }

 private:
  composition::Namespace::ID id_;
  composition::FocusScopeBehavior behavior_;
};

class FocusSection {
 public:
  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).focus_section();
  }
};

class Disabled {
 public:
  constexpr explicit Disabled(bool is_disabled = true) noexcept
      : disabled_(is_disabled) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).disabled(disabled_);
  }

 private:
  bool disabled_;
};

class OnMoveCommand {
 public:
  explicit OnMoveCommand(composition::Command::MoveAction perform) noexcept
      : action_(std::move(perform)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).on_move_command(std::move(action_));
  }

 private:
  composition::Command::MoveAction action_;
};

class OnExitCommand {
 public:
  explicit OnExitCommand(composition::Command::Action perform)
      : label_(composition::Command::exit_label), action_(std::move(perform)) {}

  OnExitCommand(std::string label,
                composition::Command::Action perform) noexcept
      : label_(std::move(label)), action_(std::move(perform)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).on_exit_command(std::move(label_),
                                              std::move(action_));
  }

 private:
  std::string label_;
  composition::Command::Action action_;
};

class OnPlayPauseCommand {
 public:
  explicit OnPlayPauseCommand(composition::Command::Action perform)
      : label_(composition::Command::play_pause_label),
        action_(std::move(perform)) {}

  OnPlayPauseCommand(std::string label,
                     composition::Command::Action perform) noexcept
      : label_(std::move(label)), action_(std::move(perform)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).on_play_pause_command(std::move(label_),
                                                    std::move(action_));
  }

 private:
  std::string label_;
  composition::Command::Action action_;
};

class OnCommand {
 public:
  OnCommand(provider::Button button, std::string label,
            composition::Command::Action perform,
            composition::CommandOptions options = {}) noexcept
      : button_(button),
        label_(std::move(label)),
        action_(std::move(perform)),
        options_(options) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).on_command(button_, std::move(label_),
                                         std::move(action_), options_);
  }

 private:
  provider::Button button_;
  std::string label_;
  composition::Command::Action action_;
  composition::CommandOptions options_;
};

class SensoryFeedbackAttribute : public Deferred {
 public:
  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  SensoryFeedbackAttribute(composition::SensoryFeedback feedback, T trigger)
      : Deferred([feedback, trigger = std::move(trigger)](
                     composition::Element element) mutable {
          return std::move(element).sensory_feedback(feedback,
                                                     std::move(trigger));
        }) {}

  template <std::equality_comparable T,
            composition::FeedbackResolver<T> Resolve>
    requires std::copy_constructible<T>
  SensoryFeedbackAttribute(T trigger, Resolve resolve)
      : Deferred([trigger = std::move(trigger), resolve = std::move(resolve)](
                     composition::Element element) mutable {
          return std::move(element).sensory_feedback(std::move(trigger),
                                                     std::move(resolve));
        }) {}
};

using focusable = Focusable;
using focus_effect_disabled = FocusEffectDisabled;
using focused = Focused;
using default_focus = DefaultFocus;
using prefers_default_focus = PrefersDefaultFocus;
using focus_scope = FocusScope;
using focus_section = FocusSection;
using disabled = Disabled;
using on_move_command = OnMoveCommand;
using on_exit_command = OnExitCommand;
using on_play_pause_command = OnPlayPauseCommand;
using on_command = OnCommand;
using sensory_feedback = SensoryFeedbackAttribute;
using sensory_feedback_disabled = SensoryFeedbackDisabled;

}
