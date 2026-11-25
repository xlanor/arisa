#include <arisa/graphics/rotation_3d.hpp>

#include <algorithm>
#include <cmath>

namespace arisa::graphics {

Rotation3D::Rotation3D(motion::Angle angle, Axis axis, motion::UnitPoint anchor,
                       float anchor_z, float perspective) noexcept
    : angle_(angle),
      axis_(axis),
      anchor_(anchor),
      anchor_z_(anchor_z),
      perspective_(perspective) {}

auto Rotation3D::projection(composition::Size size) const noexcept
    -> ProjectionTransform {
  const float length = std::sqrt((axis_.x * axis_.x) + (axis_.y * axis_.y) +
                                 (axis_.z * axis_.z));
  if (length <= 0.0F || angle_.radians() == 0.0) {
    return ProjectionTransform::identity();
  }
  const float kx = axis_.x / length;
  const float ky = axis_.y / length;
  const float kz = axis_.z / length;
  const auto c = static_cast<float>(std::cos(angle_.radians()));
  const auto s = static_cast<float>(std::sin(angle_.radians()));
  const float t = 1.0F - c;
  const std::array<float, 9> r{
      (c + (kx * kx * t)),        ((kx * ky * t) - (kz * s)),
      ((kx * kz * t) + (ky * s)), ((ky * kx * t) + (kz * s)),
      (c + (ky * ky * t)),        ((ky * kz * t) - (kx * s)),
      ((kz * kx * t) - (ky * s)), ((kz * ky * t) + (kx * s)),
      (c + (kz * kz * t))};
  const float ax = anchor_.x * size.width;
  const float ay = anchor_.y * size.height;
  const float z0 = -anchor_z_;
  const float depth = std::max(size.width, size.height);
  const float inverse =
      perspective_ > 0.0F && depth > 0.0F ? perspective_ / depth : 0.0F;
  const float wx = -r[6] * inverse;
  const float wy = -r[7] * inverse;
  const float w1 = 1.0F - (((r[8] * z0) + anchor_z_) * inverse);
  const ProjectionTransform rotated{std::array<float, 9>{
      (ax * wx) + r[0], (ay * wx) + r[3], wx, (ax * wy) + r[1],
      (ay * wy) + r[4], wy, (ax * w1) + (r[2] * z0), (ay * w1) + (r[5] * z0),
      w1}};
  return ProjectionTransform{AffineTransform::translation(-ax, -ay)}
      .concatenating(rotated);
}

}
