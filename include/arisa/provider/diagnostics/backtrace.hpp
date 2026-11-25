#pragma once

#include <arisa/provider/diagnostics/memory_space.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace arisa::provider {

class Backtrace {
 public:
  static constexpr std::size_t capacity = 32;

  static auto walk(const MemorySpace& memory, std::optional<std::uint64_t> pc,
                   std::uint64_t fp) noexcept -> Backtrace;

  auto frames() const noexcept -> std::span<const std::uint64_t>;

 private:
  static auto record(const MemorySpace& memory, std::uint64_t fp) noexcept
      -> bool;

  void push(std::uint64_t address) noexcept;

  std::array<std::uint64_t, capacity> frames_{};
  std::size_t count_{0};
};

}
