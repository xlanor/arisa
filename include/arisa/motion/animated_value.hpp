#pragma once

#include <arisa/motion/animatable_vector.hpp>
#include <arisa/motion/animation_track.hpp>
#include <arisa/motion/presentation.hpp>

#include <chrono>
#include <optional>

namespace arisa::motion {

class AnimatedValue {
 public:
  auto animate(const AnimatableVector& target,
               const Presentation::Moment& moment) -> AnimatableVector;

  auto animating() const noexcept -> bool { return track_.has_value(); }

  auto presented() const noexcept -> const AnimatableVector& {
    return presented_;
  }

 private:
  AnimatableVector target_;
  AnimatableVector presented_;
  AnimatableVector velocity_;
  std::optional<AnimationTrack> track_;
  bool ready_{false};
};

}
