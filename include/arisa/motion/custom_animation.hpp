#pragma once

#include <arisa/motion/animatable_vector.hpp>
#include <arisa/motion/animation_context.hpp>

#include <optional>

namespace arisa::motion {

class Animation;

class CustomAnimation {
 public:
  virtual ~CustomAnimation() = default;

  virtual auto animate(const AnimatableVector& value, double time,
                       AnimationContext& context) const
      -> std::optional<AnimatableVector> = 0;

  virtual auto velocity(const AnimatableVector& value, double time,
                        const AnimationContext& context) const
      -> std::optional<AnimatableVector>;

  virtual auto should_merge(const Animation& previous,
                            const AnimatableVector& value, double time,
                            AnimationContext& context) const -> bool;
};

}
