#pragma once

#include <arisa/composition/rect.hpp>
#include <arisa/graphics/blend_mode.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/image_paint.hpp>
#include <arisa/graphics/shadow_style.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/material.hpp>

#include <cstdint>
#include <optional>
#include <utility>
#include <variant>

namespace arisa::theme {
class Theme;
}

namespace arisa::graphics {

struct LinearGradient {
  Gradient gradient;
  motion::UnitPoint start_point{motion::UnitPoint::leading};
  motion::UnitPoint end_point{motion::UnitPoint::trailing};

  friend auto operator==(const LinearGradient&, const LinearGradient&)
      -> bool = default;
};

struct RadialGradient {
  Gradient gradient;
  motion::UnitPoint center{motion::UnitPoint::center};
  float start_radius{0.0F};
  float end_radius{100.0F};

  friend auto operator==(const RadialGradient&, const RadialGradient&)
      -> bool = default;
};

struct AngularGradient {
  Gradient gradient;
  motion::UnitPoint center{motion::UnitPoint::center};
  motion::Angle start_angle{};
  motion::Angle end_angle{motion::Angle::degrees(360.0)};

  friend auto operator==(const AngularGradient&, const AngularGradient&)
      -> bool = default;
};

struct EllipticalGradient {
  Gradient gradient;
  motion::UnitPoint center{motion::UnitPoint::center};
  float start_radius_fraction{0.0F};
  float end_radius_fraction{0.5F};

  friend auto operator==(const EllipticalGradient&, const EllipticalGradient&)
      -> bool = default;
};

enum class HierarchicalShapeStyle : std::uint8_t {
  primary,
  secondary,
  tertiary,
  quaternary,
  quinary,
};

struct BackgroundStyle {
  friend constexpr auto operator==(const BackgroundStyle&,
                                   const BackgroundStyle&) -> bool = default;
};

class ShapeStyle {
 public:
  using Value =
      std::variant<theme::ColourValue, LinearGradient, RadialGradient,
                   AngularGradient, EllipticalGradient, theme::Material,
                   HierarchicalShapeStyle, BackgroundStyle, ImagePaint>;

  ShapeStyle(theme::ColourValue colour) : value_(colour) {}

  ShapeStyle(theme::ColourRole role) : value_(theme::ColourValue{role}) {}

  ShapeStyle(theme::Colour colour) : value_(theme::ColourValue{colour}) {}

  ShapeStyle(LinearGradient gradient) : value_(std::move(gradient)) {}

  ShapeStyle(RadialGradient gradient) : value_(std::move(gradient)) {}

  ShapeStyle(AngularGradient gradient) : value_(std::move(gradient)) {}

  ShapeStyle(EllipticalGradient gradient) : value_(std::move(gradient)) {}

  ShapeStyle(theme::Material material) : value_(material) {}

  ShapeStyle(HierarchicalShapeStyle level) : value_(level) {}

  ShapeStyle(BackgroundStyle background) : value_(background) {}

  ShapeStyle(ImagePaint paint) : value_(std::move(paint)) {}

  ShapeStyle(Gradient gradient)
      : value_(LinearGradient{.gradient = std::move(gradient),
                              .start_point = motion::UnitPoint::top,
                              .end_point = motion::UnitPoint::bottom}) {}

  ShapeStyle(const ShapeStyle& other);

  ShapeStyle(ShapeStyle&& other) noexcept;

  auto operator=(const ShapeStyle& other) -> ShapeStyle&;

  auto operator=(ShapeStyle&& other) noexcept -> ShapeStyle&;

  ~ShapeStyle();

  static auto black() -> ShapeStyle;

  static auto white() -> ShapeStyle;

  static auto bar() -> ShapeStyle { return ShapeStyle{theme::Material::bar}; }

  static auto image(std::shared_ptr<const provider::Image> image,
                    ImagePaint::SourceRect source_rect = {}, float scale = 1.0F)
      -> ShapeStyle;

  static auto blended(BlendMode mode) -> ShapeStyle;

  static auto foreground() -> ShapeStyle {
    return ShapeStyle{HierarchicalShapeStyle::primary};
  }

  static auto background() -> ShapeStyle {
    return ShapeStyle{BackgroundStyle{}};
  }

  auto value() const noexcept -> const Value& { return value_; }

  auto gradient() const noexcept -> const Gradient*;

  auto colour() const noexcept -> std::optional<theme::ColourValue>;

  auto level() const noexcept -> std::optional<HierarchicalShapeStyle>;

  auto material() const noexcept -> std::optional<theme::Material>;

  auto image_paint() const noexcept -> const ImagePaint*;

  auto opacity() const noexcept -> float { return opacity_; }

  auto opacity(float amount) const -> ShapeStyle;

  auto shadow() const noexcept -> const std::optional<ShadowStyle>& {
    return shadow_;
  }

  auto shadow(ShadowStyle style) const -> ShapeStyle;

  auto substituted(const ShapeStyle& replacement) const -> ShapeStyle;

  auto blend_mode() const noexcept -> BlendMode { return blend_; }

  auto blend_mode(BlendMode mode) const -> ShapeStyle;

  auto bounds() const noexcept -> const std::optional<composition::Rect>& {
    return bounds_;
  }

  auto in(composition::Rect rect) const -> ShapeStyle;

  auto resolve(const theme::Theme& theme, theme::ColorScheme scheme) const
      -> ShapeStyle;

  friend auto operator==(const ShapeStyle&, const ShapeStyle&)
      -> bool = default;

 private:
  Value value_;
  float opacity_{1.0F};
  std::optional<ShadowStyle> shadow_;
  BlendMode blend_{BlendMode::normal};
  std::optional<composition::Rect> bounds_;
};

}
