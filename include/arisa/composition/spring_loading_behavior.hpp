#pragma once

#include <arisa/composition/environment_key.hpp>

#include <chrono>
#include <cstdint>

namespace arisa::composition {

enum class SpringLoadingBehavior : std::uint8_t {
  automatic,
  enabled,
  disabled,
};

class SpringLoading {
 public:
  static constexpr std::chrono::milliseconds delay{700};

  static const EnvironmentKey<SpringLoadingBehavior> behavior;
};

}
