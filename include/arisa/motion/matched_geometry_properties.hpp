#pragma once

#include <cstdint>

namespace arisa::motion {

class MatchedGeometryProperties {
 public:
  static const MatchedGeometryProperties position;
  static const MatchedGeometryProperties size;
  static const MatchedGeometryProperties frame;
  static const MatchedGeometryProperties corner_radius;
  static const MatchedGeometryProperties colour;

  constexpr auto contains(MatchedGeometryProperties other) const noexcept
      -> bool {
    return (bits_ & other.bits_) == other.bits_;
  }

  friend constexpr auto operator|(MatchedGeometryProperties left,
                                  MatchedGeometryProperties right) noexcept
      -> MatchedGeometryProperties {
    return MatchedGeometryProperties{
        static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator==(const MatchedGeometryProperties&,
                                   const MatchedGeometryProperties&)
      -> bool = default;

 private:
  constexpr explicit MatchedGeometryProperties(std::uint8_t bits) noexcept
      : bits_(bits) {}

  std::uint8_t bits_;
};

inline constexpr MatchedGeometryProperties MatchedGeometryProperties::position{
    1U};
inline constexpr MatchedGeometryProperties MatchedGeometryProperties::size{2U};
inline constexpr MatchedGeometryProperties MatchedGeometryProperties::frame{3U};
inline constexpr MatchedGeometryProperties
    MatchedGeometryProperties::corner_radius{4U};
inline constexpr MatchedGeometryProperties MatchedGeometryProperties::colour{
    8U};

}
