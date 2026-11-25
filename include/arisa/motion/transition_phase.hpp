#pragma once

#include <cstdint>

namespace arisa::motion {

class TransitionPhase {
 public:
  enum class Value : std::int8_t {
    will_appear = -1,
    identity = 0,
    did_disappear = 1,
  };

  static constexpr auto will_appear() noexcept -> TransitionPhase {
    return TransitionPhase{Value::will_appear};
  }

  static constexpr auto identity() noexcept -> TransitionPhase {
    return TransitionPhase{Value::identity};
  }

  static constexpr auto did_disappear() noexcept -> TransitionPhase {
    return TransitionPhase{Value::did_disappear};
  }

  constexpr auto is_identity() const noexcept -> bool {
    return value_ == Value::identity;
  }

  constexpr auto value() const noexcept -> double {
    return static_cast<double>(static_cast<std::int8_t>(value_));
  }

  constexpr auto phase() const noexcept -> Value { return value_; }

  friend constexpr auto operator==(TransitionPhase, TransitionPhase)
      -> bool = default;

 private:
  constexpr explicit TransitionPhase(Value value) noexcept : value_(value) {}

  Value value_;
};

}
