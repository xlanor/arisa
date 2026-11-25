#pragma once

#include <cstdint>

namespace arisa::composition {

class EventModifiers {
 public:
  constexpr EventModifiers() noexcept = default;

  static const EventModifiers command;
  static const EventModifiers control;
  static const EventModifiers option;
  static const EventModifiers shift;
  static const EventModifiers caps_lock;
  static const EventModifiers numeric_pad;
  static const EventModifiers function;
  static const EventModifiers all;

  constexpr auto contains(EventModifiers other) const noexcept -> bool {
    return (bits_ & other.bits_) == other.bits_;
  }

  constexpr auto empty() const noexcept -> bool { return bits_ == 0; }

  friend constexpr auto operator|(EventModifiers left,
                                  EventModifiers right) noexcept
      -> EventModifiers {
    return EventModifiers{static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator&(EventModifiers left,
                                  EventModifiers right) noexcept
      -> EventModifiers {
    return EventModifiers{static_cast<std::uint8_t>(left.bits_ & right.bits_)};
  }

  friend constexpr auto operator==(EventModifiers, EventModifiers)
      -> bool = default;

 private:
  constexpr explicit EventModifiers(std::uint8_t bits) noexcept : bits_(bits) {}

  std::uint8_t bits_{0};
};

inline constexpr EventModifiers EventModifiers::command{1U << 0U};
inline constexpr EventModifiers EventModifiers::control{1U << 1U};
inline constexpr EventModifiers EventModifiers::option{1U << 2U};
inline constexpr EventModifiers EventModifiers::shift{1U << 3U};
inline constexpr EventModifiers EventModifiers::caps_lock{1U << 4U};
inline constexpr EventModifiers EventModifiers::numeric_pad{1U << 5U};
inline constexpr EventModifiers EventModifiers::function{1U << 6U};
inline constexpr EventModifiers EventModifiers::all{0x7FU};

}
