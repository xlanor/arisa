#pragma once

#include <arisa/composition/size.hpp>
#include <arisa/graphics/projection_transform.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/unit_point.hpp>

#include <array>

namespace arisa::graphics {

class Rotation3D {
 public:
  struct Axis {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};

    friend constexpr auto operator==(const Axis&, const Axis&)
        -> bool = default;
  };

  Rotation3D(motion::Angle angle, Axis axis,
             motion::UnitPoint anchor = motion::UnitPoint::center,
             float anchor_z = 0.0F, float perspective = 1.0F) noexcept;

  auto projection(composition::Size size) const noexcept -> ProjectionTransform;

  auto angle() const noexcept -> motion::Angle { return angle_; }

  auto axis() const noexcept -> Axis { return axis_; }

  auto anchor() const noexcept -> motion::UnitPoint { return anchor_; }

  auto anchor_z() const noexcept -> float { return anchor_z_; }

  auto perspective() const noexcept -> float { return perspective_; }

  friend auto operator==(const Rotation3D&, const Rotation3D&)
      -> bool = default;

 private:
  motion::Angle angle_;
  Axis axis_;
  motion::UnitPoint anchor_;
  float anchor_z_;
  float perspective_;
};

}
