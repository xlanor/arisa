#pragma once

#include <cstdint>

namespace arisa::composition {

class PresentationPlacement {
 public:
  enum class Kind : std::uint8_t {
    automatic,
    center,
    leading,
    trailing,
  };

  static const PresentationPlacement automatic;
  static const PresentationPlacement center;
  static const PresentationPlacement leading;
  static const PresentationPlacement trailing;

  constexpr auto kind() const noexcept -> Kind { return kind_; }

  friend constexpr auto operator==(PresentationPlacement, PresentationPlacement)
      -> bool = default;

 private:
  constexpr explicit PresentationPlacement(Kind kind) noexcept : kind_(kind) {}

  Kind kind_;
};

inline constexpr PresentationPlacement PresentationPlacement::automatic{
    Kind::automatic};
inline constexpr PresentationPlacement PresentationPlacement::center{
    Kind::center};
inline constexpr PresentationPlacement PresentationPlacement::leading{
    Kind::leading};
inline constexpr PresentationPlacement PresentationPlacement::trailing{
    Kind::trailing};

}
