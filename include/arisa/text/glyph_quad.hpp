#pragma once

#include <arisa/provider/gpu/texture.hpp>

#include <cstdint>

namespace arisa::text {

struct GlyphQuad {
  provider::TextureId texture{0};
  std::uint32_t page{0};
  float left{0.0F};
  float top{0.0F};
  float right{0.0F};
  float bottom{0.0F};
  float u0{0.0F};
  float v0{0.0F};
  float u1{0.0F};
  float v1{0.0F};
};

}
