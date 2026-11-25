#pragma once

#include <cstdint>

namespace arisa::provider {

struct GpuResources {
  std::uint64_t textures_created{0};
  std::uint64_t memory_blocks_created{0};

  friend constexpr auto operator==(const GpuResources&, const GpuResources&)
      -> bool = default;
};

}
