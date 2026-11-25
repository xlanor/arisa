#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace arisa::composition {

enum class CommandTrigger : std::uint8_t {
  press,
  repeat,
  long_press,
};

struct CommandOptions {
  CommandTrigger trigger{CommandTrigger::press};
  bool hidden{false};
  std::optional<std::chrono::milliseconds> hold{};

  friend constexpr auto operator==(const CommandOptions&, const CommandOptions&)
      -> bool = default;
};

}
