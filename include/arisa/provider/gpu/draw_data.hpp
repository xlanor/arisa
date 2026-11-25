#pragma once

#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/gpu/vertex.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <span>

namespace arisa::provider {

struct ClipRect {
  std::uint32_t x{0};
  std::uint32_t y{0};
  std::uint32_t width{0};
  std::uint32_t height{0};

  friend constexpr auto operator==(const ClipRect&, const ClipRect&)
      -> bool = default;
};

struct DrawCommand {
  static constexpr std::array<float, 2> untrimmed{-1.0e9F, 1.0e9F};

  TextureId texture{0};
  ClipRect clip{};
  std::uint32_t index_offset{0};
  std::uint32_t index_count{0};
  std::uint32_t vertex_offset{0};
  TextureId detail{0};
  std::array<float, 2> trim{untrimmed};
  std::array<float, 2> translation{};
  float opacity{1.0F};

  auto placement() const noexcept -> std::array<float, 4> {
    return {translation[0], translation[1], opacity, 0.0F};
  }

  auto translation_for(ShapeKind kind) const noexcept -> std::array<float, 2> {
    if (kind != ShapeKind::glyph) {
      return translation;
    }
    return {std::round(translation[0]), std::round(translation[1])};
  }

  friend constexpr auto operator==(const DrawCommand&, const DrawCommand&)
      -> bool = default;
};

template <class V>
struct Geometry {
  std::span<V> vertices;
  std::span<Index> indices;
  std::uint32_t block{0};
  std::uint32_t first_vertex{0};
  std::uint32_t first_index{0};
};

using FrameGeometry = Geometry<Vertex>;
using ShapeGeometry = Geometry<ShapeVertex>;

template <class V>
struct BasicDrawData {
  Geometry<V> geometry;
  std::span<const DrawCommand> commands;
};

using DrawData = BasicDrawData<Vertex>;
using ShapeDrawData = BasicDrawData<ShapeVertex>;

struct VertexRange {
  std::uint32_t first{0};
  std::uint32_t count{0};

  friend constexpr auto operator==(const VertexRange&, const VertexRange&)
      -> bool = default;
};

struct ReplayDrawData {
  std::span<const Index> indices;
  std::span<const DrawCommand> commands;
  std::uint32_t vertex_count{0};
  std::uint64_t generation{0};
};

struct ResidentShapes {
  static constexpr std::uint32_t block = 0xFFFF'FFFFU;

  ShapeGeometry geometry;
  std::uint64_t generation{0};
};

}
