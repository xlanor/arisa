#pragma once

#include <cstdint>

namespace arisa::components {

class PinnedScrollableViews {
 public:
  static const PinnedScrollableViews section_headers;
  static const PinnedScrollableViews section_footers;

  constexpr PinnedScrollableViews() noexcept = default;

  constexpr auto empty() const noexcept -> bool { return bits_ == 0; }

  constexpr auto contains(PinnedScrollableViews other) const noexcept -> bool {
    return (bits_ & other.bits_) == other.bits_ && other.bits_ != 0;
  }

  friend constexpr auto operator|(PinnedScrollableViews left,
                                  PinnedScrollableViews right) noexcept
      -> PinnedScrollableViews {
    return PinnedScrollableViews{
        static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator==(PinnedScrollableViews, PinnedScrollableViews)
      -> bool = default;

 private:
  constexpr explicit PinnedScrollableViews(std::uint8_t bits) noexcept
      : bits_(bits) {}

  std::uint8_t bits_{0};
};

inline constexpr PinnedScrollableViews PinnedScrollableViews::section_headers{
    1U};
inline constexpr PinnedScrollableViews PinnedScrollableViews::section_footers{
    2U};

}
