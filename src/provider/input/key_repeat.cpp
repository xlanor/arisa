#include <arisa/provider/input/key_repeat.hpp>

#include <algorithm>

namespace arisa::provider {

auto KeyRepeat::modifier(Key key) noexcept -> bool {
  switch (key) {
    case Key::left_ctrl:
    case Key::right_ctrl:
    case Key::left_shift:
    case Key::right_shift:
    case Key::left_alt:
    case Key::right_alt:
    case Key::left_super:
    case Key::right_super:
      return true;
    default:
      return false;
  }
}

auto KeyRepeat::update(std::span<const Key> held, std::chrono::nanoseconds now)
    -> std::optional<Key> {
  for (const Key key : held) {
    if (!modifier(key) && !std::ranges::contains(previous_, key)) {
      repeating_ = key;
      next_ = now + delay_;
    }
  }
  previous_.assign(held.begin(), held.end());
  if (!repeating_.has_value() ||
      !std::ranges::contains(previous_, *repeating_)) {
    repeating_.reset();
    return std::nullopt;
  }
  if (now < next_) {
    return std::nullopt;
  }
  next_ = now + interval_;
  return repeating_;
}

}
