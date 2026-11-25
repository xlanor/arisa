#pragma once

#include <cstdint>
#include <optional>

namespace arisa::provider {

struct HeapUsage {
  std::optional<std::uint64_t> blocks;
  std::uint64_t bytes{0};

  friend constexpr auto operator==(const HeapUsage&, const HeapUsage&)
      -> bool = default;
};

}
