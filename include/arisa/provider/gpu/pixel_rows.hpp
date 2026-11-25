#pragma once

#include <concepts>
#include <cstddef>
#include <span>

namespace arisa::provider {

class PixelRows {
 public:
  constexpr PixelRows() noexcept = default;

  template <class Bytes>
    requires std::convertible_to<Bytes&&, std::span<const std::byte>>
  constexpr PixelRows(Bytes&& bytes) noexcept
      : bytes_(std::span<const std::byte>{bytes}) {}

  static constexpr auto strided(std::span<const std::byte> bytes,
                                std::size_t pitch) noexcept -> PixelRows {
    PixelRows rows{bytes};
    rows.pitch_ = pitch;
    return rows;
  }

  constexpr auto bytes() const noexcept -> std::span<const std::byte> {
    return bytes_;
  }

  constexpr auto pitch() const noexcept -> std::size_t { return pitch_; }

  constexpr auto covers(std::size_t row_bytes, std::size_t rows) const noexcept
      -> bool {
    if (rows == 0 || row_bytes == 0) {
      return false;
    }
    if (pitch_ == 0) {
      return bytes_.size() == row_bytes * rows;
    }
    return pitch_ >= row_bytes &&
           bytes_.size() >= ((rows - 1) * pitch_) + row_bytes;
  }

  constexpr auto row(std::size_t index, std::size_t row_bytes) const noexcept
      -> std::span<const std::byte> {
    const std::size_t stride = pitch_ == 0 ? row_bytes : pitch_;
    return bytes_.subspan(index * stride, row_bytes);
  }

 private:
  std::span<const std::byte> bytes_;
  std::size_t pitch_{0};
};

}
