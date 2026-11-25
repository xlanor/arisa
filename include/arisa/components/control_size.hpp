#pragma once

#include <array>
#include <cstdint>

namespace arisa::components {

enum class ControlSize : std::uint8_t {
  mini,
  small,
  regular,
  large,
  extra_large,
};

inline constexpr std::array<ControlSize, 5> all_control_sizes{
    ControlSize::mini, ControlSize::small, ControlSize::regular,
    ControlSize::large, ControlSize::extra_large};

}
