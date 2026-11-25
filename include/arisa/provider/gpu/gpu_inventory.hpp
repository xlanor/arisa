#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace arisa::provider {

struct GpuInventory {
  struct Textures {
    std::size_t sampled{0};
    std::uint64_t sampled_bytes{0};
    std::size_t targets{0};
    std::uint64_t target_bytes{0};

    friend constexpr auto operator==(const Textures&, const Textures&)
        -> bool = default;
  };

  std::string backend;
  std::string report;
  Textures textures;
  std::optional<std::uint64_t> frames_presented{};
  std::optional<std::uint64_t> missed_vsyncs{};

  friend auto operator==(const GpuInventory&, const GpuInventory&)
      -> bool = default;
};

}
