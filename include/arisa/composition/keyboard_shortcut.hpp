#pragma once

#include <arisa/composition/event_modifiers.hpp>
#include <arisa/composition/key_equivalent.hpp>

#include <cstdint>

namespace arisa::composition {

struct KeyboardShortcut {
  enum class Localization : std::uint8_t {
    automatic,
    without_mirroring,
    custom,
  };

  KeyEquivalent key{KeyEquivalent::space};
  EventModifiers modifiers{EventModifiers::command};
  Localization localization{Localization::automatic};

  static const KeyboardShortcut default_action;
  static const KeyboardShortcut cancel_action;

  constexpr auto mirrored() const noexcept -> KeyboardShortcut {
    if (localization != Localization::automatic) {
      return *this;
    }
    KeyboardShortcut flipped = *this;
    if (key == KeyEquivalent::left_arrow) {
      flipped.key = KeyEquivalent::right_arrow;
    } else if (key == KeyEquivalent::right_arrow) {
      flipped.key = KeyEquivalent::left_arrow;
    } else if (key == KeyEquivalent{'['}) {
      flipped.key = KeyEquivalent{']'};
    } else if (key == KeyEquivalent{']'}) {
      flipped.key = KeyEquivalent{'['};
    } else if (key == KeyEquivalent{'{'}) {
      flipped.key = KeyEquivalent{'}'};
    } else if (key == KeyEquivalent{'}'}) {
      flipped.key = KeyEquivalent{'{'};
    }
    return flipped;
  }

  friend constexpr auto operator==(const KeyboardShortcut&,
                                   const KeyboardShortcut&) -> bool = default;
};

inline constexpr KeyboardShortcut KeyboardShortcut::default_action{
    .key = KeyEquivalent::return_key, .modifiers = EventModifiers{}};
inline constexpr KeyboardShortcut KeyboardShortcut::cancel_action{
    .key = KeyEquivalent::escape, .modifiers = EventModifiers{}};

}
