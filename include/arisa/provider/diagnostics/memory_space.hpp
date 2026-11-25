#pragma once

#include <arisa/provider/diagnostics/memory_region.hpp>

#include <cstdint>
#include <optional>

namespace arisa::provider {

class MemorySpace {
 public:
  virtual ~MemorySpace() = default;

  virtual auto region(std::uint64_t address) const noexcept
      -> std::optional<MemoryRegion> = 0;

  auto word(std::uint64_t address) const noexcept
      -> std::optional<std::uint64_t>;

  auto half(std::uint64_t address) const noexcept
      -> std::optional<std::uint32_t>;

  auto byte(std::uint64_t address) const noexcept
      -> std::optional<std::uint8_t>;

 protected:
  MemorySpace() = default;
  MemorySpace(const MemorySpace&) = default;
  auto operator=(const MemorySpace&) -> MemorySpace& = default;
  MemorySpace(MemorySpace&&) = default;
  auto operator=(MemorySpace&&) -> MemorySpace& = default;

 private:
  virtual auto fetch(std::uint64_t address) const noexcept -> std::uint64_t = 0;
};

}
