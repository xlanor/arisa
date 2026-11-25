#pragma once

#include <cstdint>

namespace arisa::composition {

class SymbolVariants {
 public:
  static const SymbolVariants none;
  static const SymbolVariants fill;
  static const SymbolVariants circle;
  static const SymbolVariants square;
  static const SymbolVariants rectangle;
  static const SymbolVariants slash;

  constexpr SymbolVariants() noexcept = default;

  constexpr auto contains(SymbolVariants other) const noexcept -> bool {
    return (bits_ & other.bits_) == other.bits_ && other.bits_ != 0;
  }

  constexpr auto filled() const noexcept -> SymbolVariants {
    return *this | fill;
  }

  constexpr auto shape() const noexcept -> SymbolVariants {
    return SymbolVariants{static_cast<std::uint8_t>(
        bits_ & (circle.bits_ | square.bits_ | rectangle.bits_))};
  }

  friend constexpr auto operator|(SymbolVariants left,
                                  SymbolVariants right) noexcept
      -> SymbolVariants {
    return SymbolVariants{static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator==(SymbolVariants, SymbolVariants)
      -> bool = default;

 private:
  constexpr explicit SymbolVariants(std::uint8_t bits) noexcept : bits_(bits) {}

  std::uint8_t bits_{0};
};

inline constexpr SymbolVariants SymbolVariants::none{};
inline constexpr SymbolVariants SymbolVariants::fill{1U};
inline constexpr SymbolVariants SymbolVariants::circle{2U};
inline constexpr SymbolVariants SymbolVariants::square{4U};
inline constexpr SymbolVariants SymbolVariants::rectangle{8U};
inline constexpr SymbolVariants SymbolVariants::slash{16U};

}
