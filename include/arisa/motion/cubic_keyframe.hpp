#pragma once

#include <arisa/motion/animatable_traits.hpp>
#include <arisa/motion/keyframe_sequence.hpp>

#include <optional>
#include <utility>

namespace arisa::motion {

template <Animatable T>
class CubicKeyframe {
 public:
  CubicKeyframe(T to, double duration,
                std::optional<T> start_velocity = std::nullopt,
                std::optional<T> end_velocity = std::nullopt)
      : to_(std::move(to)),
        duration_(duration),
        start_velocity_(std::move(start_velocity)),
        end_velocity_(std::move(end_velocity)) {}

  auto to() const noexcept -> const T& { return to_; }

  auto duration() const noexcept -> double { return duration_; }

  auto start_velocity() const noexcept -> const std::optional<T>& {
    return start_velocity_;
  }

  auto end_velocity() const noexcept -> const std::optional<T>& {
    return end_velocity_;
  }

  auto keyframe() const -> KeyframeSequence::Keyframe {
    const auto vector = [](const std::optional<T>& velocity)
        -> std::optional<AnimatableVector> {
      if (!velocity.has_value()) {
        return std::nullopt;
      }
      return AnimatableTraits<T>::vector(*velocity);
    };
    return KeyframeSequence::Keyframe{.kind = KeyframeSequence::Kind::cubic,
                                      .to = AnimatableTraits<T>::vector(to_),
                                      .duration = duration_,
                                      .curve = UnitCurve::linear,
                                      .spring = Spring{},
                                      .start_velocity = vector(start_velocity_),
                                      .end_velocity = vector(end_velocity_)};
  }

 private:
  T to_;
  double duration_;
  std::optional<T> start_velocity_;
  std::optional<T> end_velocity_;
};

template <class T, class... Rest>
CubicKeyframe(T, Rest...) -> CubicKeyframe<T>;

}
