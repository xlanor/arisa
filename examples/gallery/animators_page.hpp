#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/motion/transition_content.hpp>
#include <arisa/motion/transition_phase.hpp>

#include <chrono>
#include <string_view>

namespace arisa::examples {

class AnimatorsPage final : public composition::Component {
 public:
  class Opener;
  class Clock;
  class Zoom;
  class Morph;
  class Demos;

  struct Spin {
    auto body(motion::TransitionContent content,
              motion::TransitionPhase phase) const -> motion::TransitionContent;
  };

  static constexpr std::string_view close_button = "animators-close";
  static constexpr std::string_view live_toggle = "animators-live";
  static constexpr std::string_view favourite_button = "animators-favourite";
  static constexpr std::string_view morph_button = "animators-morph";
  static constexpr std::string_view materialize_button =
      "animators-materialize";
  static constexpr std::string_view transition_button = "animators-transition";
  static constexpr std::string_view styles_button = "animators-styles";
  static constexpr std::string_view zoom_card = "animators-zoom-card";

  explicit AnimatorsPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const AnimatorsPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto card(std::string_view title, composition::Element content)
      -> composition::Element;

 private:
  composition::StateHandle<bool> shown_;
};

class AnimatorsPage::Opener final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"animators-shown",
                                                         false};
  static constexpr std::string_view button = "animators-open";

  auto operator==(const Opener&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

class AnimatorsPage::Clock final : public composition::Component {
 public:
  explicit Clock(std::chrono::system_clock::time_point date) noexcept
      : date_(date) {}

  auto operator==(const Clock&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  std::chrono::system_clock::time_point date_;
};

class AnimatorsPage::Zoom final : public composition::Component {
 public:
  static constexpr composition::Namespace space{"animators-zoom"};

  auto operator==(const Zoom&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

class AnimatorsPage::Morph final : public composition::Component {
 public:
  static constexpr composition::Namespace space{"animators-glass"};
  static constexpr composition::StateKey<bool> open_key{"animators-glass-open",
                                                        false};

  auto operator==(const Morph&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

class AnimatorsPage::Demos final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> live_key{"animators-live",
                                                        false};
  static constexpr composition::StateKey<int> favourite_key{
      "animators-favourite", 0};
  static constexpr composition::StateKey<bool> materialized_key{
      "animators-materialized", true};
  static constexpr composition::StateKey<bool> inserted_key{
      "animators-inserted", true};
  static constexpr composition::StateKey<bool> styled_key{"animators-styled",
                                                          false};

  auto operator==(const Demos&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
