#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/haptics/haptic_strength.hpp>
#include <arisa/provider/haptics/rumble_player.hpp>
#include <arisa/provider/haptics/rumble_value.hpp>
#include <arisa/provider/haptics/sensory_feedback.hpp>

#include <expected>

namespace arisa::provider {

class HapticsProvider {
 public:
  virtual ~HapticsProvider() = default;

  [[nodiscard]] virtual auto play(const SensoryFeedback& feedback,
                                  HapticStrength strength)
      -> std::expected<void, ProviderError> = 0;

  [[nodiscard]] virtual auto rumble(RumblePlayer, RumbleFrequency,
                                    RumbleFrequency, RumbleAmplitude,
                                    RumbleAmplitude)
      -> std::expected<void, ProviderError> {
    return std::unexpected(ProviderError::unsupported_capability());
  }
};

}
