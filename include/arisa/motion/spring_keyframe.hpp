#pragma once

#include <arisa/motion/animatable_traits.hpp>
#include <arisa/motion/keyframe_sequence.hpp>
#include <arisa/motion/spring.hpp>

#include <optional>
#include <utility>

namespace arisa::motion {

template <Animatable T>
class SpringKeyframe {
 public:
  SpringKeyframe(T to, std::optional<double> duration = std::nullopt,
                 Spring spring = Spring{},
                 std::optional<T> start_velocity = std::nullopt)
      : to_(std::move(to)),
        duration_(duration),
        spring_(spring),
        start_velocity_(std::move(start_velocity)) {}

  SpringKeyframe(T to, Spring spring)
      : SpringKeyframe(std::move(to), std::nullopt, spring) {}

  auto to() const noexcept -> const T& { return to_; }

  auto duration() const noexcept -> std::optional<double> { return duration_; }

  auto spring() const noexcept -> const Spring& { return spring_; }

  auto start_velocity() const noexcept -> const std::optional<T>& {
    return start_velocity_;
  }

  auto keyframe() const -> KeyframeSequence::Keyframe {
    std::optional<AnimatableVector> velocity;
    if (start_velocity_.has_value()) {
      velocity = AnimatableTraits<T>::vector(*start_velocity_);
    }
    return KeyframeSequence::Keyframe{.kind = KeyframeSequence::Kind::spring,
                                      .to = AnimatableTraits<T>::vector(to_),
                                      .duration = duration_,
                                      .curve = UnitCurve::linear,
                                      .spring = spring_,
                                      .start_velocity = velocity,
                                      .end_velocity = std::nullopt};
  }

 private:
  T to_;
  std::optional<double> duration_;
  Spring spring_;
  std::optional<T> start_velocity_;
};

template <class T, class... Rest>
SpringKeyframe(T, Rest...) -> SpringKeyframe<T>;

}
