#pragma once

#include <arisa/composition/size.hpp>
#include <arisa/theme/radius.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

namespace arisa::theme {

enum class RoundedCornerStyle : std::uint8_t {
  circular,
  continuous,
};

struct RectangleCornerRadii {
  float top_leading{0.0F};
  float bottom_leading{0.0F};
  float bottom_trailing{0.0F};
  float top_trailing{0.0F};

  friend constexpr auto operator==(const RectangleCornerRadii&,
                                   const RectangleCornerRadii&)
      -> bool = default;
};

class ConcentricCorner {
 public:
  enum class Kind : std::uint8_t {
    concentric,
    fixed,
  };

  constexpr ConcentricCorner() noexcept = default;

  static constexpr auto concentric(float minimum = 0.0F) noexcept
      -> ConcentricCorner {
    return ConcentricCorner{Kind::concentric, minimum};
  }

  static constexpr auto fixed(float radius) noexcept -> ConcentricCorner {
    return ConcentricCorner{Kind::fixed, radius};
  }

  constexpr auto kind() const noexcept -> Kind { return kind_; }

  constexpr auto radius() const noexcept -> float { return radius_; }

  friend constexpr auto operator==(const ConcentricCorner&,
                                   const ConcentricCorner&) -> bool = default;

 private:
  constexpr ConcentricCorner(Kind kind, float radius) noexcept
      : kind_(kind), radius_(radius) {}

  Kind kind_{Kind::concentric};
  float radius_{0.0F};
};

struct RoundedRectangularShapeCorners {
  enum class Corner : std::uint8_t {
    top_leading,
    bottom_leading,
    bottom_trailing,
    top_trailing,
  };

  ConcentricCorner top_leading{ConcentricCorner::fixed(0.0F)};
  ConcentricCorner bottom_leading{ConcentricCorner::fixed(0.0F)};
  ConcentricCorner top_trailing{ConcentricCorner::fixed(0.0F)};
  ConcentricCorner bottom_trailing{ConcentricCorner::fixed(0.0F)};

  static constexpr auto all(ConcentricCorner corner) noexcept
      -> RoundedRectangularShapeCorners {
    return RoundedRectangularShapeCorners{.top_leading = corner,
                                          .bottom_leading = corner,
                                          .top_trailing = corner,
                                          .bottom_trailing = corner};
  }

  static constexpr auto fixed(float radius) noexcept
      -> RoundedRectangularShapeCorners {
    return all(ConcentricCorner::fixed(radius));
  }

  static constexpr auto concentric(float minimum = 0.0F) noexcept
      -> RoundedRectangularShapeCorners {
    return all(ConcentricCorner::concentric(minimum));
  }

  constexpr auto operator[](Corner corner) const noexcept -> ConcentricCorner {
    switch (corner) {
      case Corner::top_leading:
        return top_leading;
      case Corner::bottom_leading:
        return bottom_leading;
      case Corner::bottom_trailing:
        return bottom_trailing;
      case Corner::top_trailing:
        return top_trailing;
    }
    return top_leading;
  }

  friend constexpr auto operator==(const RoundedRectangularShapeCorners&,
                                   const RoundedRectangularShapeCorners&)
      -> bool = default;
};

struct UniformTopCorners {
  ConcentricCorner corner;
};

struct UniformBottomCorners {
  ConcentricCorner corner;
};

struct UniformLeadingCorners {
  ConcentricCorner corner;
};

struct UniformTrailingCorners {
  ConcentricCorner corner;
};

enum class ShapeForm : std::uint8_t {
  rounded,
  ellipse,
  uneven,
  concentric,
  container_relative,
  circle,
};

struct Shape {
  Radius radius{RadiusRole::none};
  bool continuous{true};
  ShapeForm form{ShapeForm::rounded};
  RectangleCornerRadii corner_radii{};
  std::array<ConcentricCorner, 4> concentric{};
  bool uniform{false};
  composition::Size corner_extent{};
  std::uint8_t uniform_edges{0};

  static constexpr std::uint8_t uniform_top = 1U;
  static constexpr std::uint8_t uniform_bottom = 2U;
  static constexpr std::uint8_t uniform_leading = 4U;
  static constexpr std::uint8_t uniform_trailing = 8U;

  static constexpr auto rectangle() noexcept -> Shape {
    return Shape{.radius = RadiusRole::none, .continuous = false};
  }

  static constexpr auto rounded(Radius radius, bool continuous = true) noexcept
      -> Shape {
    return Shape{.radius = radius, .continuous = continuous};
  }

  static constexpr auto capsule() noexcept -> Shape {
    return Shape{.radius = RadiusRole::capsule, .continuous = false};
  }

  static constexpr auto capsule(RoundedCornerStyle style) noexcept -> Shape {
    return Shape{.radius = RadiusRole::capsule,
                 .continuous = style == RoundedCornerStyle::continuous};
  }

  static constexpr auto circle() noexcept -> Shape {
    return Shape{.radius = RadiusRole::capsule,
                 .continuous = false,
                 .form = ShapeForm::circle};
  }

  static constexpr auto rect(
      composition::Size corner_size,
      RoundedCornerStyle style = RoundedCornerStyle::continuous) noexcept
      -> Shape {
    return Shape{.radius = corner_size.width,
                 .continuous = style == RoundedCornerStyle::continuous,
                 .corner_extent = corner_size};
  }

  static constexpr auto rect(UniformTopCorners top,
                             UniformBottomCorners bottom) noexcept -> Shape {
    return concentric_corners(top.corner, top.corner, bottom.corner,
                              bottom.corner, uniform_top | uniform_bottom);
  }

  static constexpr auto rect(UniformLeadingCorners leading,
                             UniformTrailingCorners trailing) noexcept
      -> Shape {
    return concentric_corners(leading.corner, trailing.corner, leading.corner,
                              trailing.corner,
                              uniform_leading | uniform_trailing);
  }

  static constexpr auto rect(UniformTopCorners top,
                             ConcentricCorner bottom_leading_corner,
                             ConcentricCorner bottom_trailing_corner) noexcept
      -> Shape {
    return concentric_corners(top.corner, top.corner, bottom_leading_corner,
                              bottom_trailing_corner, uniform_top);
  }

  static constexpr auto rect(UniformBottomCorners bottom,
                             ConcentricCorner top_leading_corner,
                             ConcentricCorner top_trailing_corner) noexcept
      -> Shape {
    return concentric_corners(top_leading_corner, top_trailing_corner,
                              bottom.corner, bottom.corner, uniform_bottom);
  }

  static constexpr auto rect(UniformLeadingCorners leading,
                             ConcentricCorner top_trailing_corner,
                             ConcentricCorner bottom_trailing_corner) noexcept
      -> Shape {
    return concentric_corners(leading.corner, top_trailing_corner,
                              leading.corner, bottom_trailing_corner,
                              uniform_leading);
  }

  static constexpr auto rect(UniformTrailingCorners trailing,
                             ConcentricCorner top_leading_corner,
                             ConcentricCorner bottom_leading_corner) noexcept
      -> Shape {
    return concentric_corners(top_leading_corner, trailing.corner,
                              bottom_leading_corner, trailing.corner,
                              uniform_trailing);
  }

  static constexpr auto concentric_corners(ConcentricCorner top_leading,
                                           ConcentricCorner top_trailing,
                                           ConcentricCorner bottom_leading,
                                           ConcentricCorner bottom_trailing,
                                           std::uint8_t edges = 0U) noexcept
      -> Shape {
    return Shape{.radius = RadiusRole::none,
                 .continuous = false,
                 .form = ShapeForm::concentric,
                 .concentric = {top_leading, bottom_leading, bottom_trailing,
                                top_trailing},
                 .uniform_edges = edges};
  }

  constexpr auto style() const noexcept -> RoundedCornerStyle {
    return continuous ? RoundedCornerStyle::continuous
                      : RoundedCornerStyle::circular;
  }

  constexpr auto elliptical() const noexcept -> bool {
    return form == ShapeForm::rounded &&
           corner_extent.width != corner_extent.height;
  }

  constexpr auto corners(std::optional<composition::Size> size) const noexcept
      -> std::optional<RoundedRectangularShapeCorners> {
    switch (form) {
      case ShapeForm::rounded: {
        if (const float* points = radius.points()) {
          return RoundedRectangularShapeCorners::fixed(*points);
        }
        const RadiusRole role = *radius.role();
        if (role == RadiusRole::none) {
          return RoundedRectangularShapeCorners::fixed(0.0F);
        }
        if (role == RadiusRole::capsule && size.has_value()) {
          return RoundedRectangularShapeCorners::fixed(
              std::min(size->width, size->height) * 0.5F);
        }
        return std::nullopt;
      }
      case ShapeForm::circle:
        if (!size.has_value()) {
          return std::nullopt;
        }
        return RoundedRectangularShapeCorners::fixed(
            std::min(size->width, size->height) * 0.5F);
      case ShapeForm::uneven:
        return RoundedRectangularShapeCorners{
            .top_leading = ConcentricCorner::fixed(corner_radii.top_leading),
            .bottom_leading =
                ConcentricCorner::fixed(corner_radii.bottom_leading),
            .top_trailing = ConcentricCorner::fixed(corner_radii.top_trailing),
            .bottom_trailing =
                ConcentricCorner::fixed(corner_radii.bottom_trailing)};
      case ShapeForm::concentric:
        return RoundedRectangularShapeCorners{.top_leading = concentric[0],
                                              .bottom_leading = concentric[1],
                                              .top_trailing = concentric[3],
                                              .bottom_trailing = concentric[2]};
      case ShapeForm::ellipse:
      case ShapeForm::container_relative:
        break;
    }
    return std::nullopt;
  }

  static constexpr auto ellipse() noexcept -> Shape {
    return Shape{.radius = RadiusRole::capsule,
                 .continuous = false,
                 .form = ShapeForm::ellipse};
  }

  constexpr auto mirrors() const noexcept -> bool {
    return form != ShapeForm::rounded && form != ShapeForm::ellipse &&
           form != ShapeForm::circle;
  }

  constexpr auto relative() const noexcept -> bool {
    return form == ShapeForm::concentric ||
           form == ShapeForm::container_relative;
  }

  friend constexpr auto operator==(const Shape&, const Shape&)
      -> bool = default;
};

class Rectangle : public Shape {
 public:
  constexpr Rectangle() noexcept : Shape{Shape::rectangle()} {}
};

class RoundedRectangle : public Shape {
 public:
  constexpr explicit RoundedRectangle(
      Radius corner_radius,
      RoundedCornerStyle style = RoundedCornerStyle::circular) noexcept
      : Shape{Shape::rounded(corner_radius,
                             style == RoundedCornerStyle::continuous)} {}

  constexpr explicit RoundedRectangle(
      composition::Size corner_size,
      RoundedCornerStyle style = RoundedCornerStyle::circular) noexcept
      : Shape{Shape::rect(corner_size, style)} {}

  constexpr auto corner_size() const noexcept -> composition::Size {
    if (corner_extent != composition::Size{}) {
      return corner_extent;
    }
    const float* points = radius.points();
    const float side = points != nullptr ? *points : 0.0F;
    return composition::Size{.width = side, .height = side};
  }
};

class UnevenRoundedRectangle : public Shape {
 public:
  constexpr explicit UnevenRoundedRectangle(
      RectangleCornerRadii radii,
      RoundedCornerStyle style = RoundedCornerStyle::circular) noexcept
      : Shape{.radius = RadiusRole::none,
              .continuous = style == RoundedCornerStyle::continuous,
              .form = ShapeForm::uneven,
              .corner_radii = radii} {}

  constexpr UnevenRoundedRectangle(
      float top_leading_radius, float bottom_leading_radius,
      float bottom_trailing_radius, float top_trailing_radius,
      RoundedCornerStyle style = RoundedCornerStyle::circular) noexcept
      : UnevenRoundedRectangle(
            RectangleCornerRadii{.top_leading = top_leading_radius,
                                 .bottom_leading = bottom_leading_radius,
                                 .bottom_trailing = bottom_trailing_radius,
                                 .top_trailing = top_trailing_radius},
            style) {}
};

class ConcentricRectangle : public Shape {
 public:
  using Corner = ConcentricCorner;

  constexpr ConcentricRectangle(UniformTopCorners top,
                                UniformBottomCorners bottom) noexcept
      : Shape{Shape::rect(top, bottom)} {}

  constexpr ConcentricRectangle(UniformLeadingCorners leading,
                                UniformTrailingCorners trailing) noexcept
      : Shape{Shape::rect(leading, trailing)} {}

  constexpr ConcentricRectangle(UniformTopCorners top,
                                Corner bottom_leading_corner,
                                Corner bottom_trailing_corner) noexcept
      : Shape{Shape::rect(top, bottom_leading_corner, bottom_trailing_corner)} {
  }

  constexpr ConcentricRectangle(UniformBottomCorners bottom,
                                Corner top_leading_corner,
                                Corner top_trailing_corner) noexcept
      : Shape{Shape::rect(bottom, top_leading_corner, top_trailing_corner)} {}

  constexpr ConcentricRectangle(UniformLeadingCorners leading,
                                Corner top_trailing_corner,
                                Corner bottom_trailing_corner) noexcept
      : Shape{Shape::rect(leading, top_trailing_corner,
                          bottom_trailing_corner)} {}

  constexpr ConcentricRectangle(UniformTrailingCorners trailing,
                                Corner top_leading_corner,
                                Corner bottom_leading_corner) noexcept
      : Shape{
            Shape::rect(trailing, top_leading_corner, bottom_leading_corner)} {}

  constexpr explicit ConcentricRectangle(Corner corners = Corner::concentric(),
                                         bool is_uniform = false) noexcept
      : Shape{.radius = RadiusRole::none,
              .continuous = false,
              .form = ShapeForm::concentric,
              .concentric = {corners, corners, corners, corners},
              .uniform = is_uniform} {}

  constexpr ConcentricRectangle(Corner top_leading_corner,
                                Corner top_trailing_corner,
                                Corner bottom_leading_corner,
                                Corner bottom_trailing_corner) noexcept
      : Shape{.radius = RadiusRole::none,
              .continuous = false,
              .form = ShapeForm::concentric,
              .concentric = {top_leading_corner, bottom_leading_corner,
                             bottom_trailing_corner, top_trailing_corner}} {}
};

class ContainerRelativeShape : public Shape {
 public:
  constexpr ContainerRelativeShape() noexcept
      : Shape{.radius = RadiusRole::none,
              .continuous = false,
              .form = ShapeForm::container_relative} {}
};

class Capsule : public Shape {
 public:
  constexpr Capsule() noexcept : Shape{Shape::capsule()} {}

  constexpr explicit Capsule(RoundedCornerStyle style) noexcept
      : Shape{Shape::capsule(style)} {}
};

class Circle : public Shape {
 public:
  constexpr Circle() noexcept : Shape{Shape::circle()} {}
};

class Ellipse : public Shape {
 public:
  constexpr Ellipse() noexcept : Shape{Shape::ellipse()} {}
};

}
