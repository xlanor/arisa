#include <arisa/provider/diagnostics/memory_space.hpp>

namespace arisa::provider {

auto MemorySpace::word(std::uint64_t address) const noexcept
    -> std::optional<std::uint64_t> {
  if (address % sizeof(std::uint64_t) != 0) {
    return std::nullopt;
  }
  const std::optional<MemoryRegion> found = region(address);
  if (!found.has_value() || !found->readable ||
      !found->holds(address, sizeof(std::uint64_t))) {
    return std::nullopt;
  }
  return fetch(address);
}

auto MemorySpace::half(std::uint64_t address) const noexcept
    -> std::optional<std::uint32_t> {
  if (address % sizeof(std::uint32_t) != 0) {
    return std::nullopt;
  }
  const std::uint64_t shift = (address % sizeof(std::uint64_t)) * 8U;
  return word(address - address % sizeof(std::uint64_t))
      .transform([shift](std::uint64_t value) {
        return static_cast<std::uint32_t>(value >> shift);
      });
}

auto MemorySpace::byte(std::uint64_t address) const noexcept
    -> std::optional<std::uint8_t> {
  const std::uint64_t shift = (address % sizeof(std::uint64_t)) * 8U;
  return word(address - address % sizeof(std::uint64_t))
      .transform([shift](std::uint64_t value) {
        return static_cast<std::uint8_t>(value >> shift);
      });
}

}
