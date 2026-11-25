#pragma once

#include <cstddef>

namespace arisa::text {

struct AtlasStatistics {
  std::size_t glyphs{0};
  std::size_t rasterised{0};
  std::size_t evictions{0};
  std::size_t pages{0};
  std::size_t uploads{0};
  std::size_t uploaded_bytes{0};
};

}
