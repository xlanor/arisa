#pragma once

#include <arisa/motion/animatable_traits.hpp>
#include <arisa/motion/keyframe_sequence.hpp>
#include <arisa/motion/unit_curve.hpp>

#include <utility>

namespace arisa::motion {

template <Animatable T>
class LinearKeyframe {
 public:
  LinearKeyframe(T to, double duration,
                 UnitCurve timing_curve = UnitCurve::linear)
      : to_(std::move(to)), duration_(duration), curve_(timing_curve) {}

  auto to() const noexcept -> const T& { return to_; }

  auto duration() const noexcept -> double { return duration_; }

  auto timing_curve() const noexcept -> const UnitCurve& { return curve_; }

  auto keyframe() const -> KeyframeSequence::Keyframe {
    return KeyframeSequence::Keyframe{.kind = KeyframeSequence::Kind::linear,
                                      .to = AnimatableTraits<T>::vector(to_),
                                      .duration = duration_,
                                      .curve = curve_,
                                      .spring = Spring{},
                                      .start_velocity = std::nullopt,
                                      .end_velocity = std::nullopt};
  }

 private:
  T to_;
  double duration_;
  UnitCurve curve_;
};

template <class T, class... Rest>
LinearKeyframe(T, Rest...) -> LinearKeyframe<T>;

}
