#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace arisa::composition {

enum class ContentMode : std::uint8_t {
  fit,
  fill,
};

inline constexpr std::array<ContentMode, 2> all_content_modes{
    ContentMode::fit, ContentMode::fill};

struct AspectRatio {
  std::optional<float> ratio;
  ContentMode mode{ContentMode::fit};

  friend auto operator==(const AspectRatio&, const AspectRatio&)
      -> bool = default;
};

}
