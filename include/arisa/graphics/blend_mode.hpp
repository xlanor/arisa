#pragma once

#include <cstdint>

namespace arisa::graphics {

enum class BlendMode : std::uint8_t {
  normal,
  multiply,
  screen,
  overlay,
  darken,
  lighten,
  color_dodge,
  color_burn,
  soft_light,
  hard_light,
  difference,
  exclusion,
  hue,
  saturation,
  color,
  luminosity,
  source_atop,
  destination_over,
  destination_out,
  plus_darker,
  plus_lighter,
};

}
