#include <arisa/motion/custom_animation.hpp>

namespace arisa::motion {

auto CustomAnimation::velocity(const AnimatableVector&, double,
                               const AnimationContext&) const
    -> std::optional<AnimatableVector> {
  return std::nullopt;
}

auto CustomAnimation::should_merge(const Animation&, const AnimatableVector&,
                                   double, AnimationContext&) const -> bool {
  return false;
}

}
