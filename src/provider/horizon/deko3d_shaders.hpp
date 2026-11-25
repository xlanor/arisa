#pragma once

#include <cstdint>
#include <span>

namespace arisa::provider::horizon {

class Deko3dShaders {
 public:
  static auto textured_vertex() noexcept -> std::span<const std::uint8_t>;

  static auto textured_fragment() noexcept -> std::span<const std::uint8_t>;

  static auto shape_vertex() noexcept -> std::span<const std::uint8_t>;

  static auto shape_fragment() noexcept -> std::span<const std::uint8_t>;
};

}
