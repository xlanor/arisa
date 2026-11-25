#pragma once

#include <arisa/provider/diagnostics/loaded_module.hpp>
#include <arisa/provider/diagnostics/memory_region.hpp>
#include <arisa/provider/diagnostics/memory_space.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace arisa::provider {

class LoadedModules {
 public:
  static constexpr std::size_t capacity = 32;
  static constexpr std::size_t build_id_search = 0x10000;
  static constexpr std::uint32_t build_id_note = 3;
  static constexpr std::uint32_t gnu_name = 0x00554E47;

  static auto discover(const MemorySpace& memory,
                       std::uint64_t main_base) noexcept -> LoadedModules;

  auto modules() const noexcept -> std::span<const LoadedModule>;

  auto find(std::uint64_t address) const noexcept
      -> std::optional<LoadedModule>;

  auto main() const noexcept -> std::optional<LoadedModule>;

 private:
  static auto image(const MemorySpace& memory, const MemoryRegion& text,
                    std::uint64_t main_base) noexcept -> LoadedModule;
  static void name(const MemorySpace& memory, const MemoryRegion& rodata,
                   LoadedModule& module) noexcept;
  static void build_id(const MemorySpace& memory, const MemoryRegion& rodata,
                       LoadedModule& module) noexcept;
  static void fallback_name(LoadedModule& module) noexcept;

  std::array<LoadedModule, capacity> modules_{};
  std::size_t count_{0};
};

}
