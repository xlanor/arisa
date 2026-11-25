#pragma once

#include <cstdint>
#include <string_view>

namespace arisa::text {

class FontName {
 public:
  constexpr explicit FontName(std::string_view name) noexcept
      : digest_(digest(name)) {}

  constexpr auto value() const noexcept -> std::uint64_t { return digest_; }

  friend constexpr auto operator==(FontName, FontName) -> bool = default;

 private:
  static constexpr std::uint64_t offset_basis = 14695981039346656037ULL;
  static constexpr std::uint64_t prime = 1099511628211ULL;

  static constexpr auto digest(std::string_view name) noexcept
      -> std::uint64_t {
    std::uint64_t hash = offset_basis;
    for (const char character : name) {
      hash ^= static_cast<std::uint8_t>(character);
      hash *= prime;
    }
    return hash;
  }

  std::uint64_t digest_;
};

}
