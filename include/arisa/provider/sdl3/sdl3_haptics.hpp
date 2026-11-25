#pragma once

#include <arisa/provider/haptics/provider.hpp>
#include <arisa/provider/sdl3/sdl3_context.hpp>

#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>

namespace arisa::provider::sdl3 {

class Sdl3Haptics final : public HapticsProvider {
 public:
  struct Rumble {
    std::uint16_t low;
    std::uint16_t high;
    std::chrono::milliseconds duration;
  };

  [[nodiscard]] static auto create()
      -> std::expected<Sdl3Haptics, ProviderError>;

  Sdl3Haptics(const Sdl3Haptics&) = delete;
  auto operator=(const Sdl3Haptics&) -> Sdl3Haptics& = delete;
  Sdl3Haptics(Sdl3Haptics&& other) noexcept;
  auto operator=(Sdl3Haptics&& other) noexcept -> Sdl3Haptics&;
  ~Sdl3Haptics() override;

  [[nodiscard]] auto play(const SensoryFeedback& feedback,
                          HapticStrength strength)
      -> std::expected<void, ProviderError> override;

  [[nodiscard]] auto rumble(RumblePlayer player, RumbleFrequency low_frequency,
                            RumbleFrequency high_frequency,
                            RumbleAmplitude low_amplitude,
                            RumbleAmplitude high_amplitude)
      -> std::expected<void, ProviderError> override;

  static auto rumble(const SensoryFeedback& feedback,
                     HapticStrength strength) noexcept -> Rumble;

  static constexpr std::chrono::milliseconds raw_hold{1000};

  static auto raw(const RumbleValue& value) noexcept -> Rumble;

 private:
  class Motor;

  Sdl3Haptics(Sdl3Context gamepads, std::unique_ptr<Motor> motor) noexcept;

  Sdl3Context gamepads_;
  std::unique_ptr<Motor> motor_;
};

}
