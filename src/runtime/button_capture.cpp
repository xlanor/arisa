#include <arisa/runtime/button_capture.hpp>

#include <algorithm>
#include <functional>
#include <ranges>

namespace arisa::runtime {

void ButtonCapture::Held::store(
    const std::array<provider::PadState, provider::max_pads>& pads) {
  const std::scoped_lock lock{mutex_};
  pads_ = pads;
}

auto ButtonCapture::Held::read() const
    -> std::array<provider::PadState, provider::max_pads> {
  const std::scoped_lock lock{mutex_};
  return pads_;
}

auto ButtonCapture::held() const -> provider::Buttons {
  if (!release_.valid()) {
    return {};
  }
  const std::array<provider::PadState, provider::max_pads> pads = held_->read();
  return std::ranges::fold_left(
      pads | std::views::transform(&provider::PadState::buttons),
      provider::Buttons{}, std::bit_or<>{});
}

auto ButtonCapture::held(std::size_t pad) const -> provider::Buttons {
  if (!release_.valid() || pad >= provider::max_pads) {
    return {};
  }
  return held_->read()[pad].buttons;
}

auto ButtonCapture::connected(std::size_t pad) const -> bool {
  if (!release_.valid() || pad >= provider::max_pads) {
    return false;
  }
  return held_->read()[pad].connected;
}

auto ButtonCapture::pad(std::size_t pad) const -> provider::PadState {
  if (!release_.valid() || pad >= provider::max_pads) {
    return {};
  }
  return held_->read()[pad];
}

void ButtonCapture::release() noexcept { release_.reset(); }

}
