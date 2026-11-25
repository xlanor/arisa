#include <arisa/provider/input/input_tuning.hpp>

#include <algorithm>
#include <cmath>

namespace arisa::provider {

auto InputTuning::create(float deadzone) noexcept
    -> std::optional<InputTuning> {
  return create(deadzone, NavigationTuning{});
}

auto InputTuning::create(float deadzone, NavigationTuning navigation) noexcept
    -> std::optional<InputTuning> {
  if (!std::isfinite(deadzone) || deadzone < 0.0F || deadzone >= 1.0F) {
    return std::nullopt;
  }
  const bool timed = navigation.repeat_delay.count() >= 0 &&
                     navigation.repeat_interval.count() > 0 &&
                     navigation.repeat_minimum.count() > 0 &&
                     navigation.repeat_minimum <= navigation.repeat_interval &&
                     navigation.long_press.count() > 0;
  const bool accelerates = std::isfinite(navigation.repeat_acceleration) &&
                           navigation.repeat_acceleration > 0.0F &&
                           navigation.repeat_acceleration <= 1.0F;
  const bool engages = std::isfinite(navigation.stick_engage) &&
                       std::isfinite(navigation.stick_release) &&
                       navigation.stick_release >= 0.0F &&
                       navigation.stick_release <= navigation.stick_engage &&
                       navigation.stick_engage <= 1.0F;
  const bool hysteresis = std::isfinite(navigation.direction_hysteresis) &&
                          navigation.direction_hysteresis >= 1.0F;
  const bool scrolls =
      std::isfinite(navigation.scroll_speed) && navigation.scroll_speed >= 0.0F;
  if (!timed || !accelerates || !engages || !hysteresis || !scrolls) {
    return std::nullopt;
  }
  return InputTuning{deadzone, navigation};
}

auto InputTuning::apply(Stick raw) const noexcept -> Stick {
  if (!std::isfinite(raw.x) || !std::isfinite(raw.y)) {
    return {};
  }
  const float magnitude = std::hypot(raw.x, raw.y);
  if (magnitude <= deadzone_) {
    return {};
  }
  const float scaled =
      std::min((magnitude - deadzone_) / (1.0F - deadzone_), 1.0F);
  const float factor = scaled / magnitude;
  return Stick{.x = raw.x * factor, .y = raw.y * factor};
}

}
