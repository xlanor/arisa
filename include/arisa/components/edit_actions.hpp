#pragma once

#include <cstdint>

namespace arisa::components {

class EditActions {
 public:
  static const EditActions move;
  static const EditActions remove;
  static const EditActions all;

  constexpr EditActions() noexcept = default;

  constexpr auto contains(EditActions other) const noexcept -> bool {
    return (bits_ & other.bits_) == other.bits_ && other.bits_ != 0;
  }

  friend constexpr auto operator|(EditActions left, EditActions right) noexcept
      -> EditActions {
    return EditActions{static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator==(EditActions, EditActions) -> bool = default;

 private:
  constexpr explicit EditActions(std::uint8_t bits) noexcept : bits_(bits) {}

  std::uint8_t bits_{0};
};

inline constexpr EditActions EditActions::move{1U};
inline constexpr EditActions EditActions::remove{2U};
inline constexpr EditActions EditActions::all{3U};

}
