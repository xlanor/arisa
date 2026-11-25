#pragma once

#include <array>
#include <cstdint>

namespace arisa::composition {

enum class ImageScale : std::uint8_t {
  small,
  medium,
  large,
};

enum class SymbolRenderingMode : std::uint8_t {
  monochrome,
  hierarchical,
  palette,
  multicolor,
};

enum class SymbolColorRenderingMode : std::uint8_t {
  flat,
  gradient,
};

enum class SymbolVariableValueMode : std::uint8_t {
  color,
  draw,
};

enum class ImageDynamicRange : std::uint8_t {
  standard,
  constrained_high,
  high,
};

enum class ImageResizingMode : std::uint8_t {
  tile,
  stretch,
};

enum class ImageOrientation : std::uint8_t {
  up,
  up_mirrored,
  down,
  down_mirrored,
  left,
  left_mirrored,
  right,
  right_mirrored,
};

inline constexpr std::array<ImageOrientation, 8> all_image_orientations{
    ImageOrientation::up,    ImageOrientation::up_mirrored,
    ImageOrientation::down,  ImageOrientation::down_mirrored,
    ImageOrientation::left,  ImageOrientation::left_mirrored,
    ImageOrientation::right, ImageOrientation::right_mirrored};

}
