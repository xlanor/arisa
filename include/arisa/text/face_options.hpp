#pragma once

#include <arisa/text/font_weight.hpp>

#include <cstdint>
#include <optional>

namespace arisa::text {

struct FaceOptions {
  std::uint32_t index{0};
  std::optional<FontWeight> weight;
  std::optional<float> optical_size;
  float stretch{1.0F};
  float slant{0.0F};
};

}
