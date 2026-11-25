#pragma once

#include <arisa/composition/size.hpp>
#include <arisa/graphics/affine_transform.hpp>
#include <arisa/graphics/blend_mode.hpp>
#include <arisa/graphics/colour_matrix.hpp>
#include <arisa/graphics/projection_transform.hpp>
#include <arisa/graphics/rotation_3d.hpp>
#include <arisa/graphics/shader.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/colour.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace arisa::composition {

class VisualEffect {
 public:
  constexpr VisualEffect() noexcept = default;

  constexpr auto scale_effect(
      float scale,
      motion::UnitPoint anchor = motion::UnitPoint::center) const noexcept
      -> VisualEffect {
    return scale_effect(scale, scale, anchor);
  }

  constexpr auto scale_effect(
      float x, float y,
      motion::UnitPoint anchor = motion::UnitPoint::center) const noexcept
      -> VisualEffect {
    VisualEffect next = *this;
    next.scale_x_ *= x;
    next.scale_y_ *= y;
    next.scale_anchor_ = anchor;
    return next;
  }

  constexpr auto offset(float x, float y = 0.0F) const noexcept
      -> VisualEffect {
    VisualEffect next = *this;
    next.offset_x_ += x;
    next.offset_y_ += y;
    return next;
  }

  constexpr auto rotation_effect(
      motion::Angle angle,
      motion::UnitPoint anchor = motion::UnitPoint::center) const noexcept
      -> VisualEffect {
    VisualEffect next = *this;
    next.rotation_ = next.rotation_ + angle;
    next.rotation_anchor_ = anchor;
    return next;
  }

  constexpr auto opacity(float value) const noexcept -> VisualEffect {
    VisualEffect next = *this;
    next.opacity_ *= std::clamp(value, 0.0F, 1.0F);
    return next;
  }

  auto brightness(double amount) const noexcept -> VisualEffect {
    return adjusted(graphics::ColourMatrix::brightness(amount));
  }

  auto contrast(double amount) const noexcept -> VisualEffect {
    return adjusted(graphics::ColourMatrix::contrast(amount));
  }

  auto saturation(double amount) const noexcept -> VisualEffect {
    return adjusted(graphics::ColourMatrix::saturation(amount));
  }

  auto grayscale(double amount) const noexcept -> VisualEffect {
    return adjusted(graphics::ColourMatrix::grayscale(amount));
  }

  auto hue_rotation(motion::Angle angle) const noexcept -> VisualEffect {
    return adjusted(graphics::ColourMatrix::hue_rotation(angle));
  }

  auto color_multiply(theme::Colour colour) const noexcept -> VisualEffect {
    return adjusted(graphics::ColourMatrix::multiplied(colour));
  }

  auto blur(float radius) const noexcept -> VisualEffect {
    VisualEffect next = *this;
    const float added = std::max(radius, 0.0F);
    next.blur_ = std::sqrt((blur_ * blur_) + (added * added));
    return next;
  }

  auto blur(float radius, bool opaque) const noexcept -> VisualEffect {
    VisualEffect next = blur(radius);
    next.blur_opaque_ = opaque;
    return next;
  }

  constexpr auto blend_mode(graphics::BlendMode mode) const noexcept
      -> VisualEffect {
    VisualEffect next = *this;
    next.blend_ = mode;
    return next;
  }

  auto rotation_3d_effect(motion::Angle angle, graphics::Rotation3D::Axis axis,
                          motion::UnitPoint anchor = motion::UnitPoint::center,
                          float anchor_z = 0.0F,
                          float perspective = 1.0F) const noexcept
      -> VisualEffect {
    VisualEffect next = *this;
    next.rotation_3d_ =
        graphics::Rotation3D{angle, axis, anchor, anchor_z, perspective};
    return next;
  }

  auto transform_effect(graphics::AffineTransform transform) const noexcept
      -> VisualEffect {
    VisualEffect next = *this;
    next.projection_ = next.projection_.concatenating(
        graphics::ProjectionTransform{transform});
    return next;
  }

  auto projection_effect(graphics::ProjectionTransform transform) const noexcept
      -> VisualEffect {
    VisualEffect next = *this;
    next.projection_ = next.projection_.concatenating(transform);
    return next;
  }

  auto color_effect(graphics::Shader shader, bool is_enabled = true) const
      -> VisualEffect {
    return shaded(std::move(shader), Size{}, is_enabled);
  }

  auto distortion_effect(graphics::Shader shader, Size max_sample_offset,
                         bool is_enabled = true) const -> VisualEffect {
    return shaded(std::move(shader), max_sample_offset, is_enabled);
  }

  auto layer_effect(graphics::Shader shader, Size max_sample_offset,
                    bool is_enabled = true) const -> VisualEffect {
    return shaded(std::move(shader), max_sample_offset, is_enabled);
  }

  auto shader() const noexcept -> const std::optional<graphics::Shader>& {
    return shader_;
  }

  auto sample_offset() const noexcept -> Size { return sample_offset_; }

  auto colour_matrix() const noexcept -> const graphics::ColourMatrix& {
    return colour_;
  }

  auto blur_radius() const noexcept -> float { return blur_; }

  constexpr auto blur_opaque() const noexcept -> bool { return blur_opaque_; }

  constexpr auto blend() const noexcept -> graphics::BlendMode {
    return blend_;
  }

  auto rotation_3d() const noexcept
      -> const std::optional<graphics::Rotation3D>& {
    return rotation_3d_;
  }

  auto projection() const noexcept -> const graphics::ProjectionTransform& {
    return projection_;
  }

  auto graphic() const noexcept -> bool {
    return !colour_.is_identity() || blur_ > 0.0F || rotation_3d_.has_value() ||
           !projection_.is_identity() || shader_.has_value();
  }

  constexpr auto scale_x() const noexcept -> float { return scale_x_; }

  constexpr auto scale_y() const noexcept -> float { return scale_y_; }

  constexpr auto scale_anchor() const noexcept -> motion::UnitPoint {
    return scale_anchor_;
  }

  constexpr auto offset_x() const noexcept -> float { return offset_x_; }

  constexpr auto offset_y() const noexcept -> float { return offset_y_; }

  constexpr auto rotation() const noexcept -> motion::Angle {
    return rotation_;
  }

  constexpr auto rotation_anchor() const noexcept -> motion::UnitPoint {
    return rotation_anchor_;
  }

  constexpr auto opacity_value() const noexcept -> float { return opacity_; }

  friend auto operator==(const VisualEffect&, const VisualEffect&)
      -> bool = default;

 private:
  auto shaded(graphics::Shader shader, Size offset, bool is_enabled) const
      -> VisualEffect {
    VisualEffect next = *this;
    if (is_enabled) {
      next.shader_ = std::move(shader);
      next.sample_offset_ = offset;
    }
    return next;
  }

  auto adjusted(const graphics::ColourMatrix& matrix) const noexcept
      -> VisualEffect {
    VisualEffect next = *this;
    next.colour_ = colour_.then(matrix);
    return next;
  }

  float scale_x_{1.0F};
  float scale_y_{1.0F};
  motion::UnitPoint scale_anchor_{motion::UnitPoint::center};
  float offset_x_{0.0F};
  float offset_y_{0.0F};
  motion::Angle rotation_{};
  motion::UnitPoint rotation_anchor_{motion::UnitPoint::center};
  float opacity_{1.0F};
  graphics::ColourMatrix colour_{};
  float blur_{0.0F};
  bool blur_opaque_{false};
  graphics::BlendMode blend_{graphics::BlendMode::normal};
  std::optional<graphics::Rotation3D> rotation_3d_{};
  graphics::ProjectionTransform projection_{};
  std::optional<graphics::Shader> shader_{};
  Size sample_offset_{};
};

}
