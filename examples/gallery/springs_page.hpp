#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/motion/transition_content.hpp>
#include <arisa/motion/transition_phase.hpp>

#include <string_view>

namespace arisa::examples {

class SpringsPage final : public composition::Component {
 public:
  class Opener;
  class Demos;

  struct Develop {
    static constexpr bool properties = false;

    auto body(motion::TransitionContent content,
              motion::TransitionPhase phase) const -> motion::TransitionContent;
  };

  struct Flip {
    auto body(motion::TransitionContent content,
              motion::TransitionPhase phase) const -> motion::TransitionContent;
  };

  static constexpr std::string_view close_button = "springs-close";
  static constexpr std::string_view launch_button = "springs-launch";
  static constexpr std::string_view curves_button = "springs-curves";
  static constexpr std::string_view transitions_button = "springs-transitions";
  static constexpr std::string_view attached_button = "springs-attached";
  static constexpr std::string_view binding_toggle = "springs-binding";
  static constexpr std::string_view gradient_button = "springs-gradient";

  explicit SpringsPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const SpringsPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  composition::StateHandle<bool> shown_;
};

class SpringsPage::Demos final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> launched_key{"springs-launched",
                                                            false};
  static constexpr composition::StateKey<bool> curved_key{"springs-curved",
                                                          false};
  static constexpr composition::StateKey<bool> inserted_key{"springs-inserted",
                                                            true};
  static constexpr composition::StateKey<bool> attached_key{"springs-attached",
                                                            true};
  static constexpr composition::StateKey<bool> bound_key{"springs-bound",
                                                         false};
  static constexpr composition::StateKey<bool> warm_key{"springs-warm", false};

  auto operator==(const Demos&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto card(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto puck(float shift) -> composition::Element;

  static auto lane(std::string_view name, composition::Element shown)
      -> composition::Element;

  static auto velocities(composition::StateHandle<bool> launched)
      -> composition::Element;

  static auto curves(composition::StateHandle<bool> curved)
      -> composition::Element;

  static auto transitions(composition::StateHandle<bool> inserted)
      -> composition::Element;

  static auto attached(composition::StateHandle<bool> shown)
      -> composition::Element;

  static auto binding(composition::StateHandle<bool> bound)
      -> composition::Element;

  static auto gradient(composition::StateHandle<bool> warm)
      -> composition::Element;
};

class SpringsPage::Opener final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"springs-shown",
                                                         false};
  static constexpr std::string_view button = "springs-open";

  auto operator==(const Opener&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
