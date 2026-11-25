#pragma once

#include <arisa/composition/environment_key.hpp>

#include <cstdint>
#include <optional>

namespace arisa::composition {

enum class BarPlacement : std::uint8_t {
  in_layout,
  overlay,
};

class BarPlacements {
 public:
  static const EnvironmentKey<std::optional<BarPlacement>> header;
  static const EnvironmentKey<std::optional<BarPlacement>> footer;
};

}
