#pragma once

#include <arisa/composition/environment_values.hpp>
#include <arisa/motion/animatable_vector.hpp>
#include <arisa/motion/animation_state.hpp>
#include <arisa/motion/animation_state_key.hpp>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace arisa::motion {

class AnimationContext {
 public:
  auto is_logically_complete() const noexcept -> bool {
    return logically_complete_;
  }

  void set_logically_complete(bool complete) noexcept {
    logically_complete_ = complete;
  }

  auto environment() const noexcept -> const composition::EnvironmentValues& {
    return environment_;
  }

  void set_environment(composition::EnvironmentValues environment) noexcept {
    environment_ = std::move(environment);
  }

  auto state() noexcept -> AnimatableVector& { return state_; }

  auto state() const noexcept -> const AnimatableVector& { return state_; }

  template <class T>
  auto state(const AnimationStateKey<T>& key) const -> const T& {
    return states_[key];
  }

  template <class T>
  void set_state(const AnimationStateKey<T>& key, T value) {
    states_.set(key, std::move(value));
  }

  auto animation_state() const noexcept -> const AnimationState& {
    return states_;
  }

  auto with_state(AnimationState state) const -> AnimationContext {
    AnimationContext changed = *this;
    changed.states_ = std::move(state);
    return changed;
  }

 private:
  AnimatableVector state_;
  composition::EnvironmentValues environment_;
  AnimationState states_;
  bool logically_complete_{false};
};

}
