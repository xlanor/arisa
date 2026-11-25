#include <arisa/provider/headless/recording_haptics.hpp>

#include <ranges>

namespace arisa::provider::headless {

auto RecordingHaptics::play(const SensoryFeedback& feedback,
                            HapticStrength strength)
    -> std::expected<void, ProviderError> {
  const std::scoped_lock lock{mutex_};
  played_.push_back(Played{.feedback = feedback, .strength = strength});
  return {};
}

auto RecordingHaptics::played() const -> std::vector<SensoryFeedback> {
  const std::scoped_lock lock{mutex_};
  return played_ | std::views::transform(&Played::feedback) |
         std::ranges::to<std::vector>();
}

auto RecordingHaptics::strengths() const -> std::vector<HapticStrength> {
  const std::scoped_lock lock{mutex_};
  return played_ | std::views::transform(&Played::strength) |
         std::ranges::to<std::vector>();
}

auto RecordingHaptics::rumble(RumblePlayer player,
                              RumbleFrequency low_frequency,
                              RumbleFrequency high_frequency,
                              RumbleAmplitude low_amplitude,
                              RumbleAmplitude high_amplitude)
    -> std::expected<void, ProviderError> {
  const std::scoped_lock lock{mutex_};
  rumbles_.push_back(Rumble{.player = player,
                            .value = RumbleValue{low_frequency, high_frequency,
                                                 low_amplitude, high_amplitude}
                                         .clamped()});
  return {};
}

auto RecordingHaptics::rumbles() const -> std::vector<Rumble> {
  const std::scoped_lock lock{mutex_};
  return rumbles_;
}

void RecordingHaptics::clear() {
  const std::scoped_lock lock{mutex_};
  played_.clear();
  rumbles_.clear();
}

}
