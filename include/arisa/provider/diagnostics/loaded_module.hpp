#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace arisa::provider {

struct LoadedModule {
  static constexpr std::size_t name_capacity = 48;
  static constexpr std::size_t build_id_capacity = 32;

  std::uint64_t base{0};
  std::uint64_t size{0};
  std::array<char, name_capacity> name{};
  std::size_t name_length{0};
  std::array<std::uint8_t, build_id_capacity> build_id{};
  std::size_t build_id_length{0};
  bool main{false};

  auto label() const noexcept -> std::string_view {
    return {name.data(), name_length};
  }

  auto build_id_bytes() const noexcept -> std::span<const std::uint8_t> {
    return std::span{build_id}.first(build_id_length);
  }

  auto contains(std::uint64_t address) const noexcept -> bool {
    return address >= base && address - base < size;
  }
};

}
