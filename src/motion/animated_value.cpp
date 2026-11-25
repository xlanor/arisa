#include <arisa/motion/animated_value.hpp>

#include <utility>

namespace arisa::motion {

auto AnimatedValue::animate(const AnimatableVector& target,
                            const Presentation::Moment& moment)
    -> AnimatableVector {
  if (!ready_) {
    target_ = target;
    presented_ = target;
    ready_ = true;
    return presented_;
  }
  if (target != target_) {
    const Transaction& transaction = moment.transaction;
    std::optional<Animation> animation = transaction.animation();
    if (animation.has_value() && moment.reduce_motion) {
      animation = animation->reduced();
    }
    if (animation.has_value() &&
        !(track_.has_value() &&
          track_->merge(*animation, target, moment.now))) {
      AnimationTrack fresh{*animation,
                           Animation::Span{.from = presented_,
                                           .to = target,
                                           .velocity = velocity_,
                                           .precision = 1.0e-3},
                           moment.now, transaction.completion()};
      fresh.hold_until_logically_complete(transaction.logical_completion());
      track_ = std::move(fresh);
    } else if (!animation.has_value()) {
      track_.reset();
    }
    target_ = target;
  }
  if (!track_.has_value()) {
    presented_ = target_;
    velocity_ = AnimatableVector{};
    return presented_;
  }
  const Animation::Sample sample = track_->step(moment.now);
  if (sample.finished) {
    track_.reset();
    presented_ = target_;
    velocity_ = AnimatableVector{};
    return presented_;
  }
  presented_ = sample.value;
  velocity_ = sample.velocity;
  return presented_;
}

}
