#pragma once

#include <arisa/provider/gpu/image.hpp>

#include <cstdint>

namespace arisa::runtime {

struct CapturedFrame {
  std::uint64_t frame{0};
  provider::Image image;
};

}
