#pragma once

#include <chrono>

namespace arisa::inspector {

class TimelineScale {
 public:
  static constexpr std::chrono::nanoseconds budget{16'666'667};
  static constexpr float minimum_extent = 1.0F;

  constexpr TimelineScale(std::chrono::nanoseconds ceiling,
                          float height) noexcept
      : ceiling_(ceiling), height_(height) {}

  static constexpr auto cpu(float height) noexcept -> TimelineScale {
    return TimelineScale{budget * 2, height};
  }

  static constexpr auto gpu(float height) noexcept -> TimelineScale {
    return TimelineScale{budget / 4, height};
  }

  constexpr auto ceiling() const noexcept -> std::chrono::nanoseconds {
    return ceiling_;
  }

  constexpr auto height() const noexcept -> float { return height_; }

  constexpr auto extent(std::chrono::nanoseconds value) const noexcept
      -> float {
    if (value.count() <= 0 || ceiling_.count() <= 0) {
      return 0.0F;
    }
    if (value >= ceiling_) {
      return height_;
    }
    const float linear = height_ * static_cast<float>(value.count()) /
                         static_cast<float>(ceiling_.count());
    return linear < minimum_extent ? minimum_extent : linear;
  }

  constexpr auto clipped(std::chrono::nanoseconds value) const noexcept
      -> bool {
    return value > ceiling_;
  }

 private:
  std::chrono::nanoseconds ceiling_;
  float height_;
};

}
