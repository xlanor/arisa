#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace arisa::text {

class GlyphBitmap {
 public:
  GlyphBitmap() noexcept = default;

  GlyphBitmap(std::uint32_t width, std::uint32_t height, std::int32_t left,
              std::int32_t top, std::vector<std::byte> coverage) noexcept;

  auto width() const noexcept -> std::uint32_t { return width_; }

  auto height() const noexcept -> std::uint32_t { return height_; }

  auto left() const noexcept -> std::int32_t { return left_; }

  auto top() const noexcept -> std::int32_t { return top_; }

  auto coverage() const noexcept -> std::span<const std::byte> {
    return coverage_;
  }

  auto empty() const noexcept -> bool { return width_ == 0 || height_ == 0; }

 private:
  std::uint32_t width_{0};
  std::uint32_t height_{0};
  std::int32_t left_{0};
  std::int32_t top_{0};
  std::vector<std::byte> coverage_;
};

}
