#pragma once

#include <arisa/provider/surface/surface_size.hpp>

#include <optional>

namespace arisa::provider {

class SurfaceScale {
 public:
  static constexpr float reference_width = 1280.0F;
  static constexpr float reference_height = 720.0F;
  static constexpr float minimum = 0.25F;

  static auto create(float value) noexcept -> std::optional<SurfaceScale>;

  static auto for_pixels(SurfaceSize size) noexcept -> SurfaceScale;

  static constexpr auto unit() noexcept -> SurfaceScale {
    return SurfaceScale{1.0F};
  }

  constexpr auto value() const noexcept -> float { return value_; }

  friend constexpr auto operator==(SurfaceScale, SurfaceScale)
      -> bool = default;

 private:
  constexpr explicit SurfaceScale(float value) noexcept : value_(value) {}

  float value_;
};

}
