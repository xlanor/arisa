#pragma once

#include <cstdint>

namespace arisa::provider {

struct MemoryRegion {
  std::uint64_t base{0};
  std::uint64_t size{0};
  bool readable{false};
  bool writable{false};
  bool executable{false};
  bool module_image{false};

  auto end() const noexcept -> std::uint64_t { return base + size; }

  auto holds(std::uint64_t address, std::uint64_t length) const noexcept
      -> bool {
    return address >= base && length <= size && address - base <= size - length;
  }

  friend auto operator==(const MemoryRegion&, const MemoryRegion&)
      -> bool = default;
};

}
