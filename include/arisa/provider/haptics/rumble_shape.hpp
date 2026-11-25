#pragma once

#include <arisa/provider/haptics/rumble_pulse.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <initializer_list>
#include <span>

namespace arisa::provider {

class RumbleShape {
 public:
  static constexpr std::size_t capacity = 6;

  constexpr RumbleShape() noexcept = default;

  constexpr RumbleShape(std::initializer_list<RumblePulse> pulses) noexcept {
    for (const RumblePulse& pulse : pulses) {
      if (count_ == capacity) {
        break;
      }
      pulses_[count_] = pulse;
      ++count_;
    }
  }

  constexpr auto pulses() const noexcept -> std::span<const RumblePulse> {
    return std::span{pulses_.data(), count_};
  }

  constexpr auto scaled(float factor) const noexcept -> RumbleShape {
    RumbleShape result = *this;
    for (std::size_t index = 0; index < result.count_; ++index) {
      result.pulses_[index].low_amplitude *= factor;
      result.pulses_[index].high_amplitude *= factor;
    }
    return result;
  }

  constexpr auto clamped(float ceiling) const noexcept -> RumbleShape {
    RumbleShape result = *this;
    for (std::size_t index = 0; index < result.count_; ++index) {
      result.pulses_[index].low_amplitude =
          std::min(result.pulses_[index].low_amplitude, ceiling);
      result.pulses_[index].high_amplitude =
          std::min(result.pulses_[index].high_amplitude, ceiling);
    }
    return result;
  }

  constexpr auto silent() const noexcept -> bool { return peak() <= 0.0F; }

  constexpr auto duration() const noexcept -> std::chrono::milliseconds {
    std::chrono::milliseconds total{0};
    for (const RumblePulse& pulse : pulses()) {
      total += pulse.duration;
    }
    return total;
  }

  constexpr auto peak() const noexcept -> float {
    float highest = 0.0F;
    for (const RumblePulse& pulse : pulses()) {
      highest = std::max({highest, pulse.low_amplitude, pulse.high_amplitude});
    }
    return highest;
  }

  friend constexpr auto operator==(const RumbleShape&, const RumbleShape&)
      -> bool = default;

 private:
  std::array<RumblePulse, capacity> pulses_{};
  std::size_t count_{0};
};

}
