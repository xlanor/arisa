#include <arisa/provider/horizon/nx/nx_pointer.hpp>

#include <algorithm>
#include <array>
#include <span>

namespace arisa::provider::horizon::nx {

NxPointer::NxPointer(SurfaceSize) { hidInitializeMouse(); }

auto NxPointer::position(const HidMouseState& state,
                         SurfaceSize surface) noexcept -> PixelPoint {
  const auto width = static_cast<float>(mouse_area.width.value());
  const auto height = static_cast<float>(mouse_area.height.value());
  const float x = std::clamp(static_cast<float>(state.x), 0.0F, width - 1.0F);
  const float y = std::clamp(static_cast<float>(state.y), 0.0F, height - 1.0F);
  return PixelPoint{
      .x = x * static_cast<float>(surface.width.value()) / width,
      .y = y * static_cast<float>(surface.height.value()) / height};
}

auto NxPointer::sample(SurfaceSize surface) -> PointerReading {
  std::array<HidMouseState, history> states{};
  const std::size_t count =
      std::min(hidGetMouseStates(states.data(), states.size()), states.size());
  if (count == 0) {
    if (state_.has_value()) {
      state_->wheel_x = 0.0F;
      state_->wheel_y = 0.0F;
    }
    return PointerReading{.pointer = state_};
  }
  const HidMouseState& newest = states.front();
  if ((newest.attributes & HidMouseAttribute_IsConnected) == 0) {
    state_.reset();
    sampled_ = newest.sampling_number;
    return PointerReading{};
  }
  float wheel_x = 0.0F;
  float wheel_y = 0.0F;
  for (const HidMouseState& each : std::span{states}.first(count)) {
    if (state_.has_value() && each.sampling_number <= sampled_) {
      break;
    }
    wheel_x += static_cast<float>(each.wheel_delta_x);
    wheel_y += static_cast<float>(each.wheel_delta_y);
  }
  sampled_ = newest.sampling_number;
  state_ = PointerState{.position = position(newest, surface),
                        .left = (newest.buttons & HidMouseButton_Left) != 0,
                        .right = (newest.buttons & HidMouseButton_Right) != 0,
                        .middle = (newest.buttons & HidMouseButton_Middle) != 0,
                        .wheel_x = wheel_x,
                        .wheel_y = wheel_y};
  return PointerReading{.pointer = state_};
}

}
