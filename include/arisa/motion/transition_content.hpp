#pragma once

#include <arisa/motion/angle.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/motion/transition_properties.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/colour.hpp>

namespace arisa::motion {

class TransitionContent {
 public:
  constexpr TransitionContent() noexcept = default;

  auto opacity(float value) const noexcept -> TransitionContent;

  auto scale_effect(float scale,
                    UnitPoint anchor = UnitPoint::center) const noexcept
      -> TransitionContent;

  auto scale_effect(float x, float y,
                    UnitPoint anchor = UnitPoint::center) const noexcept
      -> TransitionContent;

  auto offset(float x, float y = 0.0F) const noexcept -> TransitionContent;

  auto blur(float radius) const noexcept -> TransitionContent;

  auto rotation_effect(Angle angle) const noexcept -> TransitionContent;

  auto rotation_3d_effect(Angle angle, float x, float y, float z,
                          float perspective = 1.0F) const noexcept
      -> TransitionContent;

  auto brightness(float amount) const noexcept -> TransitionContent;

  auto contrast(float amount) const noexcept -> TransitionContent;

  auto saturation(float amount) const noexcept -> TransitionContent;

  auto grayscale(float amount) const noexcept -> TransitionContent;

  auto hue_rotation(Angle angle) const noexcept -> TransitionContent;

  auto color_multiply(theme::Colour colour) const noexcept -> TransitionContent;

  auto effect() const noexcept -> const Transition::Effect& { return effect_; }

 private:
  Transition::Effect effect_;
};

template <class T>
  requires requires(const T& custom, TransitionContent content,
                    TransitionPhase phase) {
    { custom.body(content, phase) } -> std::convertible_to<TransitionContent>;
  }
auto Transition::custom(const T& transition) -> Transition {
  Transition made{effect_of(transition.body(TransitionContent{},
                                            TransitionPhase::will_appear())),
                  effect_of(transition.body(TransitionContent{},
                                            TransitionPhase::did_disappear()))};
  if constexpr (requires {
                  requires std::constructible_from<TransitionProperties,
                                                   decltype(T::properties)>;
                }) {
    made.declared_ = TransitionProperties{T::properties};
  }
  return made;
}

}
