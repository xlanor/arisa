#pragma once

#include <arisa/motion/animatable_vector.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/animation_context.hpp>
#include <arisa/motion/custom_animation.hpp>

#include <optional>

namespace arisa::motion {

class BaseAnimation final : public CustomAnimation {
 public:
  explicit BaseAnimation(Animation animation) noexcept;

  auto animate(const AnimatableVector& value, double time,
               AnimationContext& context) const
      -> std::optional<AnimatableVector> override;

  auto velocity(const AnimatableVector& value, double time,
                const AnimationContext& context) const
      -> std::optional<AnimatableVector> override;

  auto should_merge(const Animation& previous, const AnimatableVector& value,
                    double time, AnimationContext& context) const
      -> bool override;

  auto animation() const noexcept -> const Animation& { return animation_; }

 private:
  auto sampled(const AnimatableVector& value, double time,
               AnimationContext& context) const -> Animation::Sample;

  Animation animation_;
};

}
