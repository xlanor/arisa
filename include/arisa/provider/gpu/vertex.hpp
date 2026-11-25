#pragma once

#include <array>
#include <cstdint>

namespace arisa::provider {

struct Vertex {
  std::array<float, 2> position{};
  std::array<float, 2> uv{};
  std::uint32_t colour{0};

  static constexpr auto rgba(std::uint8_t red, std::uint8_t green,
                             std::uint8_t blue, std::uint8_t alpha) noexcept
      -> std::uint32_t {
    return static_cast<std::uint32_t>(red) |
           (static_cast<std::uint32_t>(green) << 8U) |
           (static_cast<std::uint32_t>(blue) << 16U) |
           (static_cast<std::uint32_t>(alpha) << 24U);
  }

  friend constexpr auto operator==(const Vertex&, const Vertex&)
      -> bool = default;
};

static_assert(sizeof(Vertex) == 20);

using Index = std::uint32_t;

}
