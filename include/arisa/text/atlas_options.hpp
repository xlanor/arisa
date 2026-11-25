#pragma once

#include <cstdint>

namespace arisa::text {

struct AtlasOptions {
  std::uint32_t page_size{1024};
  std::uint32_t maximum_pages{4};
  std::uint32_t subpixel_steps{4};
  std::uint32_t padding{1};
};

}
