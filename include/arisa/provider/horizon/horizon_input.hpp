#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/horizon/console.hpp>
#include <arisa/provider/horizon/pointer_reading.hpp>
#include <arisa/provider/input/key_repeat.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/surface/provider.hpp>

#include <switch.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <flat_map>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace arisa::provider::horizon {

class HorizonInput final : public InputProvider {
 public:
  static constexpr float stick_extent = 32767.0F;

  explicit HorizonInput(
      const SurfaceProvider& surface, InputTuning tuning = {},
      std::shared_ptr<const std::atomic<bool>> keyboard_shown = nullptr);

  auto sample() -> InputFrame override;

  auto tuning() const noexcept -> InputTuning override;

  void set_tuning(InputTuning tuning) noexcept override;

  auto pointer_host() const noexcept -> PointerHost override;

  void present_pointer(PointerAppearance appearance) override;

  auto pointer_failure() const noexcept -> std::optional<ProviderError>;

  static void claim(std::array<PadState, max_pads>& pads,
                    const PointerReading& reading) noexcept;

  static auto translate_buttons(std::uint64_t held) noexcept -> Buttons;

  static auto normalise_stick(HidAnalogStickState stick) noexcept -> Stick;

  static auto translate_key(std::uint32_t hid) noexcept -> std::optional<Key>;

 private:
  struct TrackedTouch {
    TouchId id;
    PixelPoint position;
  };

  auto read_pad(const ::PadState& pad) const noexcept -> PadState;

  void sample_touches(InputFrame& frame);

  void release_touches(InputFrame& frame);

  void sample_keys(InputFrame& frame);

  void release_keys(InputFrame& frame);

  auto touch_position(const HidTouchState& touch) const noexcept -> PixelPoint;

  std::reference_wrapper<const SurfaceProvider> surface_;
  InputTuning tuning_;
  std::array<::PadState, max_pads> pads_{};
  std::flat_map<std::uint32_t, TrackedTouch> touches_;
  std::uint64_t next_touch_id_{1};
  std::shared_ptr<const std::atomic<bool>> keyboard_shown_;
  std::vector<Key> held_keys_;
  KeyRepeat repeat_;
  Console::Pointer pointer_;
};

}
