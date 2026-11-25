#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace arisa::provider {

enum class ShapeKind : std::uint32_t {
  glyph,
  image,
  fill,
  stroke,
  shadow,
  path,
  gradient,
  mesh,
  blur_down,
  blur_up,
  material,
  glass,
  glass_path,
  glass_union,
  layer,
  yuv,
  filter,
  blend,
  mask,
  gaussian,
  shader,
  inner_shadow,
  gradient_fill,
  glyph_gradient,
};

struct ShapeVertex {
  std::array<float, 2> position{};
  std::array<float, 2> uv{};
  std::array<float, 2> local{};
  std::array<float, 2> half_size{};
  std::array<float, 4> shape{};
  std::uint32_t colour{0};
  ShapeKind kind{ShapeKind::fill};
  std::array<float, 4> effect{};
  std::array<float, 4> detail{};

  static constexpr std::size_t radius = 0;
  static constexpr std::size_t smoothing = 1;
  static constexpr std::size_t stroke = 2;
  static constexpr std::size_t softness = 3;

  friend constexpr auto operator==(const ShapeVertex&, const ShapeVertex&)
      -> bool = default;
};

static_assert(sizeof(ShapeVertex) == 88);

}
