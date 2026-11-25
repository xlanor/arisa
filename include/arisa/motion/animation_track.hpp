#pragma once

#include <arisa/motion/animatable_vector.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/animation_completion.hpp>
#include <arisa/motion/animation_context.hpp>

#include <chrono>
#include <memory>
#include <optional>

namespace arisa::motion {

class AnimationTrack {
 public:
  AnimationTrack(Animation animation, Animation::Span span,
                 std::chrono::nanoseconds start,
                 std::shared_ptr<AnimationCompletion> completion,
                 std::optional<Spring> blended_from = std::nullopt) noexcept;

  auto sample(std::chrono::nanoseconds now) const noexcept -> Animation::Sample;

  auto step(std::chrono::nanoseconds now) noexcept -> Animation::Sample;

  auto merge(const Animation& next, const AnimatableVector& target,
             std::chrono::nanoseconds now) noexcept -> bool;

  void hold_until_logically_complete(
      std::shared_ptr<AnimationCompletion> completion) noexcept;

  auto holds_logical_completion() const noexcept -> bool {
    return logical_ != nullptr;
  }

  auto spring_at(std::chrono::nanoseconds now) const noexcept
      -> std::optional<Spring>;

  auto blended_from() const noexcept -> const std::optional<Spring>& {
    return blended_from_;
  }

  auto target() const noexcept -> const AnimatableVector& { return span_.to; }

  auto animation() const noexcept -> const Animation& { return animation_; }

  void postpone(std::chrono::nanoseconds by) noexcept { start_ += by; }

  void set_environment(composition::EnvironmentValues environment) noexcept {
    context_.set_environment(std::move(environment));
  }

  auto context() const noexcept -> const AnimationContext& { return context_; }

  auto detached() const noexcept -> AnimationTrack {
    AnimationTrack copy{animation_, span_, start_, nullptr, blended_from_};
    copy.context_ = context_;
    return copy;
  }

 private:
  auto elapsed(std::chrono::nanoseconds now) const noexcept -> double;

  Animation animation_;
  Animation::Span span_;
  std::chrono::nanoseconds start_;
  std::shared_ptr<AnimationCompletion> completion_;
  std::shared_ptr<AnimationCompletion> logical_;
  std::optional<Spring> blended_from_;
  AnimationContext context_;
};

}
