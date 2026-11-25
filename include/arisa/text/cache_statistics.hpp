#pragma once

#include <cstddef>

namespace arisa::text {

struct CacheStatistics {
  std::size_t layout_hits{0};
  std::size_t layout_misses{0};
  std::size_t shaping_hits{0};
  std::size_t shaping_misses{0};
  std::size_t layouts{0};
  std::size_t shaped{0};
  std::size_t measure_misses{0};

  friend constexpr auto operator==(const CacheStatistics&,
                                   const CacheStatistics&) -> bool = default;
};

}
