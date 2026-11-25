#pragma once

#include <cstdint>
#include <span>

namespace arisa::examples {

class PlasmaShaders {
 public:
  static auto vertex() noexcept -> std::span<const std::uint8_t>;

  static auto fragment() noexcept -> std::span<const std::uint8_t>;
};

}
