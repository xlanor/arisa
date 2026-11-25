#pragma once

#include <bit>
#include <cstdint>
#include <optional>

namespace arisa::provider::gpu {

class Alignment {
 public:
  static constexpr auto create(std::uint64_t bytes) noexcept
      -> std::optional<Alignment> {
    if (!std::has_single_bit(bytes)) {
      return std::nullopt;
    }
    return Alignment{bytes};
  }

  template <std::uint64_t Bytes>
    requires(std::has_single_bit(Bytes))
  static constexpr auto of() noexcept -> Alignment {
    return Alignment{Bytes};
  }

  constexpr auto bytes() const noexcept -> std::uint64_t { return bytes_; }

  constexpr auto up(std::uint64_t value) const noexcept -> std::uint64_t {
    return (value + bytes_ - 1) & ~(bytes_ - 1);
  }

  friend constexpr auto operator==(Alignment, Alignment) -> bool = default;
  friend constexpr auto operator<=>(Alignment, Alignment) = default;

 private:
  explicit constexpr Alignment(std::uint64_t bytes) noexcept : bytes_(bytes) {}

  std::uint64_t bytes_;
};

}
