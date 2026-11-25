#pragma once

#include <arisa/composition/event_modifiers.hpp>
#include <arisa/composition/key_equivalent.hpp>

#include <cstdint>
#include <string>
#include <utility>

namespace arisa::composition {

struct KeyPress {
  enum class Result : std::uint8_t {
    handled,
    ignored,
  };

  enum class Phases : std::uint8_t {
    down = 1,
    repeat = 2,
    up = 4,
    all = 7,
  };

  KeyEquivalent key{KeyEquivalent::space};
  std::string characters;
  EventModifiers modifiers{};
  Phases phase{Phases::down};
};

constexpr auto operator|(KeyPress::Phases left, KeyPress::Phases right) noexcept
    -> KeyPress::Phases {
  return static_cast<KeyPress::Phases>(std::to_underlying(left) |
                                       std::to_underlying(right));
}

constexpr auto operator&(KeyPress::Phases left, KeyPress::Phases right) noexcept
    -> KeyPress::Phases {
  return static_cast<KeyPress::Phases>(std::to_underlying(left) &
                                       std::to_underlying(right));
}

}
