#pragma once

#include <arisa/provider/haptics/provider.hpp>
#include <arisa/provider/haptics/rumble_queue.hpp>
#include <arisa/provider/haptics/rumble_shape.hpp>
#include <arisa/provider/horizon/service_session.hpp>
#include <arisa/provider/threading/provider.hpp>

#include <switch.h>

#include <memory>

namespace arisa::provider::horizon {

class HorizonHaptics final : public HapticsProvider {
 public:
  [[nodiscard]] static auto create(ThreadingProvider& threading)
      -> std::expected<HorizonHaptics, ProviderError>;

  [[nodiscard]] auto play(const SensoryFeedback& feedback,
                          HapticStrength strength)
      -> std::expected<void, ProviderError> override;

  [[nodiscard]] auto rumble(RumblePlayer player, RumbleFrequency low_frequency,
                            RumbleFrequency high_frequency,
                            RumbleAmplitude low_amplitude,
                            RumbleAmplitude high_amplitude)
      -> std::expected<void, ProviderError> override;

  static auto shape(const SensoryFeedback& feedback,
                    HapticStrength strength) noexcept -> RumbleShape;

  static auto vibration(const RumbleValue& value) noexcept -> HidVibrationValue;

 private:
  struct HidService {
    static auto open() noexcept -> Result { return hidInitialize(); }

    static void close() noexcept { hidExit(); }
  };

  class Motor;
  class Devices;

  HorizonHaptics(ServiceSession<HidService> hid,
                 std::shared_ptr<Devices> devices, RumbleQueue queue) noexcept;

  ServiceSession<HidService> hid_;
  std::shared_ptr<Devices> devices_;
  RumbleQueue queue_;
};

}
