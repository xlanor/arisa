#pragma once

#include <cstdint>

namespace arisa::text {

class DrawingOptions {
 public:
  static const DrawingOptions disables_subpixel_quantization;

  constexpr DrawingOptions() noexcept = default;

  constexpr explicit DrawingOptions(std::uint32_t raw_value) noexcept
      : bits_(raw_value) {}

  constexpr auto raw_value() const noexcept -> std::uint32_t { return bits_; }

  constexpr auto contains(DrawingOptions other) const noexcept -> bool {
    return other.bits_ != 0U && (bits_ & other.bits_) == other.bits_;
  }

  friend constexpr auto operator|(DrawingOptions left,
                                  DrawingOptions right) noexcept
      -> DrawingOptions {
    return DrawingOptions{left.bits_ | right.bits_};
  }

  friend constexpr auto operator==(DrawingOptions, DrawingOptions)
      -> bool = default;

 private:
  std::uint32_t bits_{0};
};

inline constexpr DrawingOptions DrawingOptions::disables_subpixel_quantization{
    1U};

}
