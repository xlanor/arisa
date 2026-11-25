#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace arisa::provider {

enum class Button : std::uint8_t {
  a,
  b,
  x,
  y,
  l,
  r,
  zl,
  zr,
  plus,
  minus,
  dpad_up,
  dpad_down,
  dpad_left,
  dpad_right,
  left_stick,
  right_stick,
  sl,
  sr,
};

inline constexpr std::array all_buttons{
    Button::a,           Button::b,          Button::x,
    Button::y,           Button::l,          Button::r,
    Button::zl,          Button::zr,         Button::plus,
    Button::minus,       Button::dpad_up,    Button::dpad_down,
    Button::dpad_left,   Button::dpad_right, Button::left_stick,
    Button::right_stick, Button::sl,         Button::sr,
};

class Buttons {
 public:
  constexpr Buttons() noexcept = default;

  constexpr Buttons(std::initializer_list<Button> pressed) noexcept {
    for (const Button button : pressed) {
      bits_ |= bit(button);
    }
  }

  constexpr auto contains(Button button) const noexcept -> bool {
    return (bits_ & bit(button)) != 0;
  }

  constexpr auto with(Button button) const noexcept -> Buttons {
    return Buttons{bits_ | bit(button)};
  }

  constexpr auto without(Button button) const noexcept -> Buttons {
    return Buttons{bits_ & ~bit(button)};
  }

  constexpr auto empty() const noexcept -> bool { return bits_ == 0; }

  constexpr auto bits() const noexcept -> std::uint32_t { return bits_; }

  friend constexpr auto operator|(Buttons left, Buttons right) noexcept
      -> Buttons {
    return Buttons{left.bits_ | right.bits_};
  }

  friend constexpr auto operator==(Buttons, Buttons) -> bool = default;

 private:
  constexpr explicit Buttons(std::uint32_t bits) noexcept : bits_(bits) {}

  static constexpr auto bit(Button button) noexcept -> std::uint32_t {
    return 1U << std::to_underlying(button);
  }

  std::uint32_t bits_{0};
};

}
