#include <arisa/provider/horizon/horizon_haptics.hpp>

#include <arisa/provider/horizon/console.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>

namespace arisa::provider::horizon {

class HorizonHaptics::Devices {
 public:
  void send(RumblePlayer player, const HidVibrationValue& value) {
    const std::scoped_lock lock{mutex_};
    if (player.is_primary()) {
      if (attached(HidNpadIdType_Handheld)) {
        send(device(HidNpadIdType_Handheld), value);
      }
      send(device(HidNpadIdType_No1), value);
      return;
    }
    if (player.is_handheld()) {
      send(device(HidNpadIdType_Handheld), value);
      return;
    }
    if (const auto index = player.index()) {
      send(device(static_cast<HidNpadIdType>(HidNpadIdType_No1 + *index)),
           value);
    }
  }

 private:
  struct Device {
    std::array<HidVibrationDeviceHandle, 2> handles{};
    s32 count{0};
    u32 styles{0};
  };

  static auto attached(HidNpadIdType id) noexcept -> bool {
    return hidGetNpadStyleSet(id) != 0;
  }

  static void send(const Device& device, const HidVibrationValue& value) {
    if (device.count <= 0) {
      return;
    }
    const std::array values{value, value};
    static_cast<void>(hidSendVibrationValues(device.handles.data(),
                                             values.data(), device.count));
  }

  auto slot(HidNpadIdType id) noexcept -> Device& {
    if (id == HidNpadIdType_Handheld) {
      return handheld_;
    }
    return players_[static_cast<std::size_t>(id - HidNpadIdType_No1) %
                    players_.size()];
  }

  auto device(HidNpadIdType id) -> Device& {
    Device& found = slot(id);
    const u32 styles = hidGetNpadStyleSet(id);
    if (styles == found.styles) {
      return found;
    }
    found.styles = styles;
    found.count = 0;
    HidNpadStyleTag style = HidNpadStyleTag_NpadFullKey;
    s32 count = 2;
    if ((styles & HidNpadStyleTag_NpadHandheld) != 0) {
      style = HidNpadStyleTag_NpadHandheld;
    } else if ((styles & HidNpadStyleTag_NpadFullKey) != 0) {
      style = HidNpadStyleTag_NpadFullKey;
    } else if ((styles & HidNpadStyleTag_NpadJoyDual) != 0) {
      style = HidNpadStyleTag_NpadJoyDual;
    } else if ((styles & HidNpadStyleTag_NpadJoyLeft) != 0) {
      style = HidNpadStyleTag_NpadJoyLeft;
      count = 1;
    } else if ((styles & HidNpadStyleTag_NpadJoyRight) != 0) {
      style = HidNpadStyleTag_NpadJoyRight;
      count = 1;
    } else {
      return found;
    }
    if (R_SUCCEEDED(hidInitializeVibrationDevices(found.handles.data(), count,
                                                  id, style))) {
      found.count = count;
    }
    return found;
  }

  std::mutex mutex_;
  Device handheld_;
  std::array<Device, RumblePlayer::players> players_{};
};

class HorizonHaptics::Motor final : public RumbleMotor {
 public:
  explicit Motor(std::shared_ptr<Devices> devices) noexcept
      : devices_(std::move(devices)) {}

  void prepare() override {}

  void vibrate(const RumblePulse& pulse) override {
    devices_->send(RumblePlayer::primary(),
                   HidVibrationValue{.amp_low = pulse.low_amplitude,
                                     .freq_low = pulse.low_frequency,
                                     .amp_high = pulse.high_amplitude,
                                     .freq_high = pulse.high_frequency});
  }

 private:
  std::shared_ptr<Devices> devices_;
};

HorizonHaptics::HorizonHaptics(ServiceSession<HidService> hid,
                               std::shared_ptr<Devices> devices,
                               RumbleQueue queue) noexcept
    : hid_(std::move(hid)),
      devices_(std::move(devices)),
      queue_(std::move(queue)) {}

auto HorizonHaptics::create(ThreadingProvider& threading)
    -> std::expected<HorizonHaptics, ProviderError> {
  auto hid = ServiceSession<HidService>::create();
  if (!hid.has_value()) {
    return std::unexpected(hid.error());
  }
  auto devices = std::make_shared<Devices>();
  auto queue = RumbleQueue::create(threading, std::make_unique<Motor>(devices));
  if (!queue.has_value()) {
    return std::unexpected(queue.error());
  }
  return HorizonHaptics{std::move(*hid), std::move(devices), std::move(*queue)};
}

auto HorizonHaptics::play(const SensoryFeedback& feedback,
                          HapticStrength strength)
    -> std::expected<void, ProviderError> {
  queue_.play(shape(feedback, strength));
  return {};
}

auto HorizonHaptics::rumble(RumblePlayer player, RumbleFrequency low_frequency,
                            RumbleFrequency high_frequency,
                            RumbleAmplitude low_amplitude,
                            RumbleAmplitude high_amplitude)
    -> std::expected<void, ProviderError> {
  devices_->send(player, vibration(RumbleValue{low_frequency, high_frequency,
                                               low_amplitude, high_amplitude}));
  return {};
}

auto HorizonHaptics::vibration(const RumbleValue& value) noexcept
    -> HidVibrationValue {
  const RumbleValue clamped = value.clamped(
      RumbleBands{.low_minimum = Console::Rumble::low_band_minimum,
                  .low_maximum = Console::Rumble::low_band_maximum,
                  .high_minimum = Console::Rumble::high_band_minimum,
                  .high_maximum = Console::Rumble::high_band_maximum,
                  .amplitude_maximum = Console::Rumble::amplitude_ceiling});
  return HidVibrationValue{.amp_low = clamped.low_amplitude().value(),
                           .freq_low = clamped.low_frequency().value(),
                           .amp_high = clamped.high_amplitude().value(),
                           .freq_high = clamped.high_frequency().value()};
}

auto HorizonHaptics::shape(const SensoryFeedback& feedback,
                           HapticStrength strength) noexcept -> RumbleShape {
  return Console::Rumble::shape(feedback, strength);
}

}
