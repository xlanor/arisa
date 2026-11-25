#include <arisa/motion/animation_track.hpp>

#include <cmath>
#include <optional>
#include <utility>

namespace arisa::motion {

AnimationTrack::AnimationTrack(Animation animation, Animation::Span span,
                               std::chrono::nanoseconds start,
                               std::shared_ptr<AnimationCompletion> completion,
                               std::optional<Spring> blended_from) noexcept
    : animation_(std::move(animation)),
      span_(span),
      start_(start),
      completion_(std::move(completion)),
      blended_from_(animation_.blend_duration() > 0.0 ? blended_from
                                                      : std::nullopt) {}

auto AnimationTrack::elapsed(std::chrono::nanoseconds now) const noexcept
    -> double {
  double seconds = std::chrono::duration<double>(now - start_).count();
  if (const std::optional<double> rate = animation_.frame_rate();
      rate.has_value() && *rate > 0.0) {
    seconds = std::floor(seconds * *rate) / *rate;
  }
  return seconds;
}

auto AnimationTrack::sample(std::chrono::nanoseconds now) const noexcept
    -> Animation::Sample {
  AnimationContext context = context_;
  return animation_.sample(span_, elapsed(now), blended_from_, context);
}

auto AnimationTrack::step(std::chrono::nanoseconds now) noexcept
    -> Animation::Sample {
  const double seconds = elapsed(now);
  Animation::Sample sampled =
      animation_.sample(span_, seconds, blended_from_, context_);
  if (logical_ != nullptr &&
      (sampled.finished || context_.is_logically_complete() ||
       seconds >= animation_.logical_duration())) {
    logical_.reset();
  }
  return sampled;
}

auto AnimationTrack::merge(const Animation& next,
                           const AnimatableVector& target,
                           std::chrono::nanoseconds now) noexcept -> bool {
  const CustomAnimation* invention = next.custom_animation();
  if (invention == nullptr ||
      !invention->should_merge(animation_, target - span_.from, elapsed(now),
                               context_)) {
    return false;
  }
  animation_ = next;
  span_.to = target;
  return true;
}

void AnimationTrack::hold_until_logically_complete(
    std::shared_ptr<AnimationCompletion> completion) noexcept {
  logical_ = std::move(completion);
}

auto AnimationTrack::spring_at(std::chrono::nanoseconds now) const noexcept
    -> std::optional<Spring> {
  return animation_.spring_at(elapsed(now), blended_from_);
}

}
