#pragma once

#include <arisa/graphics/shader.hpp>

namespace arisa::examples {

class GalleryShaders {
 public:
  static auto ripple() noexcept -> graphics::ShaderFunction;

  static auto stripes() noexcept -> graphics::ShaderFunction;

  static auto bands() noexcept -> graphics::ShaderFunction;
};

}
