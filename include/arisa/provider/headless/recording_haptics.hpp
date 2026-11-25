#pragma once

#include <arisa/provider/haptics/provider.hpp>

#include <mutex>
#include <vector>

namespace arisa::provider::headless {

class RecordingHaptics final : public HapticsProvider {
 public:
  [[nodiscard]] auto play(const SensoryFeedback& feedback,
                          HapticStrength strength)
      -> std::expected<void, ProviderError> override;

  [[nodiscard]] auto rumble(RumblePlayer player, RumbleFrequency low_frequency,
                            RumbleFrequency high_frequency,
                            RumbleAmplitude low_amplitude,
                            RumbleAmplitude high_amplitude)
      -> std::expected<void, ProviderError> override;

  struct Rumble {
    RumblePlayer player;
    RumbleValue value;
  };

  auto rumbles() const -> std::vector<Rumble>;

  auto played() const -> std::vector<SensoryFeedback>;

  auto strengths() const -> std::vector<HapticStrength>;

  void clear();

 private:
  struct Played {
    SensoryFeedback feedback;
    HapticStrength strength;
  };

  mutable std::mutex mutex_;
  std::vector<Played> played_;
  std::vector<Rumble> rumbles_;
};

}
