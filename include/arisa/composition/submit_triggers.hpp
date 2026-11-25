#pragma once

#include <cstdint>

namespace arisa::composition {

class SubmitTriggers {
 public:
  static const SubmitTriggers text;
  static const SubmitTriggers search;

  constexpr SubmitTriggers() noexcept = default;

  constexpr auto contains(SubmitTriggers other) const noexcept -> bool {
    return (bits_ & other.bits_) == other.bits_ && other.bits_ != 0U;
  }

  friend constexpr auto operator|(SubmitTriggers left,
                                  SubmitTriggers right) noexcept
      -> SubmitTriggers {
    return SubmitTriggers{static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator==(SubmitTriggers, SubmitTriggers)
      -> bool = default;

 private:
  constexpr explicit SubmitTriggers(std::uint8_t bits) noexcept : bits_(bits) {}

  std::uint8_t bits_{0};
};

inline constexpr SubmitTriggers SubmitTriggers::text{1U};
inline constexpr SubmitTriggers SubmitTriggers::search{2U};

}
