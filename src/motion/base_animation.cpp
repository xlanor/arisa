#include <arisa/motion/base_animation.hpp>

#include <optional>
#include <utility>

namespace arisa::motion {

BaseAnimation::BaseAnimation(Animation animation) noexcept
    : animation_(std::move(animation)) {}

auto BaseAnimation::sampled(const AnimatableVector& value, double time,
                            AnimationContext& context) const
    -> Animation::Sample {
  return animation_.sample(Animation::Span{.from = AnimatableVector{},
                                           .to = value,
                                           .velocity = {},
                                           .precision = 1.0e-3},
                           time, std::nullopt, context);
}

auto BaseAnimation::animate(const AnimatableVector& value, double time,
                            AnimationContext& context) const
    -> std::optional<AnimatableVector> {
  const Animation::Sample sample = sampled(value, time, context);
  if (sample.finished) {
    return std::nullopt;
  }
  return sample.value;
}

auto BaseAnimation::velocity(const AnimatableVector& value, double time,
                             const AnimationContext& context) const
    -> std::optional<AnimatableVector> {
  AnimationContext copy = context;
  const Animation::Sample sample = sampled(value, time, copy);
  if (sample.finished) {
    return std::nullopt;
  }
  return sample.velocity;
}

auto BaseAnimation::should_merge(const Animation& previous,
                                 const AnimatableVector& value, double time,
                                 AnimationContext& context) const -> bool {
  return animation_.should_merge(previous, value, time, context);
}

}
