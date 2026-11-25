#pragma once

#include <arisa/support/strong_value.hpp>

#include <algorithm>
#include <cmath>

namespace arisa::provider {

struct RumbleFrequencyTag;
struct RumbleAmplitudeTag;

using RumbleFrequency = support::StrongValue<float, RumbleFrequencyTag>;
using RumbleAmplitude = support::StrongValue<float, RumbleAmplitudeTag>;

struct RumbleBands {
  float low_minimum{40.875885F};
  float low_maximum{626.286133F};
  float high_minimum{81.75177F};
  float high_maximum{1252.572266F};
  float amplitude_maximum{1.0F};

  friend constexpr auto operator==(const RumbleBands&, const RumbleBands&)
      -> bool = default;
};

class RumbleValue {
 public:
  static constexpr float silent_low_frequency = 160.0F;
  static constexpr float silent_high_frequency = 320.0F;

  constexpr RumbleValue(RumbleFrequency low_frequency,
                        RumbleFrequency high_frequency,
                        RumbleAmplitude low_amplitude,
                        RumbleAmplitude high_amplitude) noexcept
      : low_frequency_(low_frequency),
        high_frequency_(high_frequency),
        low_amplitude_(low_amplitude),
        high_amplitude_(high_amplitude) {}

  static constexpr auto silent() noexcept -> RumbleValue {
    return RumbleValue{RumbleFrequency{silent_low_frequency},
                       RumbleFrequency{silent_high_frequency},
                       RumbleAmplitude{0.0F}, RumbleAmplitude{0.0F}};
  }

  constexpr auto low_frequency() const noexcept -> RumbleFrequency {
    return low_frequency_;
  }

  constexpr auto high_frequency() const noexcept -> RumbleFrequency {
    return high_frequency_;
  }

  constexpr auto low_amplitude() const noexcept -> RumbleAmplitude {
    return low_amplitude_;
  }

  constexpr auto high_amplitude() const noexcept -> RumbleAmplitude {
    return high_amplitude_;
  }

  auto clamped(const RumbleBands& bands = {}) const noexcept -> RumbleValue {
    return RumbleValue{
        RumbleFrequency{band(low_frequency_.value(), bands.low_minimum,
                             bands.low_maximum, silent_low_frequency)},
        RumbleFrequency{band(high_frequency_.value(), bands.high_minimum,
                             bands.high_maximum, silent_high_frequency)},
        RumbleAmplitude{
            band(low_amplitude_.value(), 0.0F, bands.amplitude_maximum, 0.0F)},
        RumbleAmplitude{band(high_amplitude_.value(), 0.0F,
                             bands.amplitude_maximum, 0.0F)}};
  }

  friend constexpr auto operator==(const RumbleValue&, const RumbleValue&)
      -> bool = default;

 private:
  static auto band(float value, float minimum, float maximum,
                   float fallback) noexcept -> float {
    if (!std::isfinite(value)) {
      return fallback;
    }
    return std::clamp(value, minimum, maximum);
  }

  RumbleFrequency low_frequency_;
  RumbleFrequency high_frequency_;
  RumbleAmplitude low_amplitude_;
  RumbleAmplitude high_amplitude_;
};

}
