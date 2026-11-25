#pragma once

#include <arisa/motion/animatable_traits.hpp>
#include <arisa/motion/keyframe_sequence.hpp>

#include <optional>
#include <utility>

namespace arisa::motion {

template <Animatable T>
class MoveKeyframe {
 public:
  explicit MoveKeyframe(T to) : to_(std::move(to)) {}

  auto to() const noexcept -> const T& { return to_; }

  auto keyframe() const -> KeyframeSequence::Keyframe {
    return KeyframeSequence::Keyframe{.kind = KeyframeSequence::Kind::move,
                                      .to = AnimatableTraits<T>::vector(to_),
                                      .duration = std::nullopt,
                                      .curve = UnitCurve::linear,
                                      .spring = Spring{},
                                      .start_velocity = std::nullopt,
                                      .end_velocity = std::nullopt};
  }

 private:
  T to_;
};

template <class T, class... Rest>
MoveKeyframe(T, Rest...) -> MoveKeyframe<T>;

}
