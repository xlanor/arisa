#pragma once

#include <string_view>
#include <utility>

namespace arisa::motion {

template <class T>
class AnimationStateKey {
 public:
  using Value = T;

  constexpr explicit AnimationStateKey(std::string_view name,
                                       T default_value = T{})
      : name_(name), default_value_(std::move(default_value)) {}

  AnimationStateKey(const AnimationStateKey&) = delete;
  auto operator=(const AnimationStateKey&) -> AnimationStateKey& = delete;
  AnimationStateKey(AnimationStateKey&&) = delete;
  auto operator=(AnimationStateKey&&) -> AnimationStateKey& = delete;
  ~AnimationStateKey() = default;

  constexpr auto name() const noexcept -> std::string_view { return name_; }

  constexpr auto default_value() const noexcept -> const T& {
    return default_value_;
  }

  auto identity() const noexcept -> const void* { return this; }

 private:
  std::string_view name_;
  T default_value_;
};

}
