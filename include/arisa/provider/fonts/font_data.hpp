#pragma once

#include <cstddef>
#include <span>

namespace arisa::provider {

class FontData {
 public:
  constexpr explicit FontData(std::span<const std::byte> bytes) noexcept
      : bytes_(bytes) {}

  constexpr auto bytes() const noexcept -> std::span<const std::byte> {
    return bytes_;
  }

  constexpr auto size() const noexcept -> std::size_t { return bytes_.size(); }

  constexpr auto empty() const noexcept -> bool { return bytes_.empty(); }

 private:
  std::span<const std::byte> bytes_;
};

}
