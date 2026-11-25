#pragma once

#include <cstdint>
#include <variant>

namespace arisa::theme {

enum class RadiusRole : std::uint8_t {
  none,
  control,
  card,
  sheet,
  capsule,
};

class Radius {
 public:
  constexpr Radius(float points) noexcept : value_(points) {}

  constexpr Radius(int points) noexcept : value_(static_cast<float>(points)) {}

  constexpr Radius(double points) noexcept
      : value_(static_cast<float>(points)) {}

  constexpr Radius(RadiusRole role) noexcept : value_(role) {}

  constexpr auto role() const noexcept -> const RadiusRole* {
    return std::get_if<RadiusRole>(&value_);
  }

  constexpr auto points() const noexcept -> const float* {
    return std::get_if<float>(&value_);
  }

  friend constexpr auto operator==(const Radius&, const Radius&)
      -> bool = default;

 private:
  std::variant<float, RadiusRole> value_;
};

namespace radius {

inline constexpr Radius none{RadiusRole::none};
inline constexpr Radius control{RadiusRole::control};
inline constexpr Radius card{RadiusRole::card};
inline constexpr Radius sheet{RadiusRole::sheet};
inline constexpr Radius capsule{RadiusRole::capsule};

}

}
