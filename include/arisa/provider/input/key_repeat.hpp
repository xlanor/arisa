#pragma once

#include <arisa/provider/input/provider.hpp>

#include <chrono>
#include <optional>
#include <span>
#include <vector>

namespace arisa::provider {

class KeyRepeat {
 public:
  static constexpr std::chrono::milliseconds default_delay{500};
  static constexpr std::chrono::milliseconds default_interval{50};

  explicit KeyRepeat(
      std::chrono::nanoseconds delay = default_delay,
      std::chrono::nanoseconds interval = default_interval) noexcept
      : delay_(delay), interval_(interval) {}

  auto update(std::span<const Key> held, std::chrono::nanoseconds now)
      -> std::optional<Key>;

 private:
  static auto modifier(Key key) noexcept -> bool;

  std::chrono::nanoseconds delay_;
  std::chrono::nanoseconds interval_;
  std::vector<Key> previous_;
  std::optional<Key> repeating_;
  std::chrono::nanoseconds next_{0};
};

}
