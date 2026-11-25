#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/motion/animation.hpp>
#include <frame_meter.hpp>

#include <functional>
#include <optional>
#include <string_view>

namespace arisa::examples {

class MotionPage final : public composition::Component {
 public:
  using MeterRef = std::optional<std::reference_wrapper<const FrameMeter>>;

  static constexpr composition::StateKey<bool> playing{"playing", false};
  static constexpr composition::StateKey<bool> toggled{"toggled", false};
  static constexpr std::string_view play_button = "motion-play";
  static constexpr std::string_view retarget_lanes = "motion-retarget";
  static constexpr std::string_view toggle_button = "motion-toggle";

  explicit MotionPage(MeterRef meter = std::nullopt) noexcept : meter_(meter) {}

  auto operator==(const MotionPage& other) const -> bool;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  struct Preset {
    std::string_view name;
    motion::Animation animation;
  };

  static auto playground(bool moved) -> composition::Element;

  static auto retargets(bool moved) -> composition::Element;

  static constexpr double outward_response = 0.9;
  static constexpr double return_response = 0.15;
  static constexpr double return_blend = 0.6;

  static auto lane(const Preset& preset, bool moved) -> composition::Element;

  static auto card(bool open) -> composition::Element;

  static auto list(bool grown) -> composition::Element;

  static auto effects(bool on) -> composition::Element;

  static auto tile(std::string_view label, composition::Element shape)
      -> composition::Element;

  static auto toggle(composition::StateHandle<bool> on) -> composition::Element;

  static auto status(const FrameMeter::Reading& reading)
      -> composition::Element;

  MeterRef meter_;
};

}
