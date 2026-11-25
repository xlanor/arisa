#pragma once

#include <arisa/theme/space.hpp>

namespace arisa::composition {

class ScenePadding {
 public:
  static const ScenePadding minimum;

  constexpr auto length() const noexcept -> theme::Space {
    return theme::Space{length_};
  }

  friend constexpr auto operator==(const ScenePadding&, const ScenePadding&)
      -> bool = default;

 private:
  constexpr explicit ScenePadding(float length) noexcept : length_(length) {}

  float length_;
};

inline constexpr ScenePadding ScenePadding::minimum{20.0F};

}
