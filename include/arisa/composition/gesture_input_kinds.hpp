#pragma once

#include <cstdint>

namespace arisa::composition {

class GestureInputKinds {
 public:
  static const GestureInputKinds direct_touch;
  static const GestureInputKinds indirect_touch;
  static const GestureInputKinds pointer;
  static const GestureInputKinds controller;
  static const GestureInputKinds all;

  constexpr GestureInputKinds() noexcept = default;

  constexpr auto contains(GestureInputKinds other) const noexcept -> bool {
    return other.bits_ != 0U && (bits_ & other.bits_) == other.bits_;
  }

  constexpr auto empty() const noexcept -> bool { return bits_ == 0U; }

  constexpr auto intersects(GestureInputKinds other) const noexcept -> bool {
    return (bits_ & other.bits_) != 0U;
  }

  friend constexpr auto operator|(GestureInputKinds left,
                                  GestureInputKinds right) noexcept
      -> GestureInputKinds {
    return GestureInputKinds{
        static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator==(GestureInputKinds, GestureInputKinds)
      -> bool = default;

 private:
  constexpr explicit GestureInputKinds(std::uint8_t bits) noexcept
      : bits_(bits) {}

  std::uint8_t bits_{0};
};

inline constexpr GestureInputKinds GestureInputKinds::direct_touch{1U};
inline constexpr GestureInputKinds GestureInputKinds::indirect_touch{2U};
inline constexpr GestureInputKinds GestureInputKinds::pointer{8U};
inline constexpr GestureInputKinds GestureInputKinds::controller{16U};
inline constexpr GestureInputKinds GestureInputKinds::all{11U};

}
