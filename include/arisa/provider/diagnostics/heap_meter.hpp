#pragma once

#include <arisa/provider/diagnostics/heap_usage.hpp>

#include <cstdint>
#include <optional>

namespace arisa::provider {

class HeapMeter {
 public:
  static auto sample() noexcept -> std::optional<HeapUsage>;

  static auto from_capacity(std::uint64_t total, std::uint64_t unused) noexcept
      -> HeapUsage;
};

}
