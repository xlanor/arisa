#pragma once

#include <cstdint>
#include <string_view>

namespace arisa::motion {

struct UnhintedAnimation {
  enum class Route : std::uint8_t {
    animation,
    hidden,
    shake,
    regroup,
  };

  Route route{Route::animation};
  std::uint64_t node{0};
  std::string_view channel{};

  static constexpr auto name(Route route) noexcept -> std::string_view {
    switch (route) {
      case Route::animation:
        return "animation";
      case Route::hidden:
        return "hidden";
      case Route::shake:
        return "shake";
      case Route::regroup:
        return "regroup";
    }
    return "animation";
  }

  friend constexpr auto operator==(const UnhintedAnimation&,
                                   const UnhintedAnimation&) -> bool = default;
};

}
