#pragma once

#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/graphics/affine_transform.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/theme/shape.hpp>

#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace arisa::graphics {

class Path {
 public:
  struct Move {
    composition::Point to;

    friend constexpr auto operator==(const Move&, const Move&)
        -> bool = default;
  };

  struct Line {
    composition::Point to;

    friend constexpr auto operator==(const Line&, const Line&)
        -> bool = default;
  };

  struct QuadCurve {
    composition::Point to;
    composition::Point control;

    friend constexpr auto operator==(const QuadCurve&, const QuadCurve&)
        -> bool = default;
  };

  struct Curve {
    composition::Point to;
    composition::Point control1;
    composition::Point control2;

    friend constexpr auto operator==(const Curve&, const Curve&)
        -> bool = default;
  };

  struct CloseSubpath {
    friend constexpr auto operator==(const CloseSubpath&, const CloseSubpath&)
        -> bool = default;
  };

  using Element = std::variant<Move, Line, QuadCurve, Curve, CloseSubpath>;

  static constexpr float continuous_reach = 1.52866483F;

  Path() = default;

  explicit Path(composition::Rect rect);

  Path(composition::Rect rounded_rect, float corner_radius,
       theme::RoundedCornerStyle style = theme::RoundedCornerStyle::continuous);

  static auto ellipse_in(composition::Rect rect) -> Path;

  static auto shaped(theme::Shape shape, composition::Rect rect, float radius)
      -> Path;

  static auto shaped(theme::Shape shape, composition::Rect rect,
                     const theme::RectangleCornerRadii& radii) -> Path;

  void move(composition::Point to);

  void add_line(composition::Point to);

  void add_lines(std::span<const composition::Point> lines);

  void add_quad_curve(composition::Point to, composition::Point control);

  void add_curve(composition::Point to, composition::Point control1,
                 composition::Point control2);

  void add_arc(composition::Point center, float radius,
               motion::Angle start_angle, motion::Angle end_angle,
               bool clockwise);

  void add_arc(composition::Point tangent1_end, composition::Point tangent2_end,
               float radius, const AffineTransform& transform = {});

  void add_relative_arc(composition::Point center, float radius,
                        motion::Angle start_angle, motion::Angle delta,
                        const AffineTransform& transform = {});

  void add_rect(composition::Rect rect);

  void add_rects(std::span<const composition::Rect> rects,
                 const AffineTransform& transform = {});

  void add_rounded_rect(
      composition::Rect rect, composition::Size corner_size,
      theme::RoundedCornerStyle style = theme::RoundedCornerStyle::continuous);

  void add_rounded_rect(
      composition::Rect rect, const theme::RectangleCornerRadii& radii,
      theme::RoundedCornerStyle style = theme::RoundedCornerStyle::continuous);

  void add_ellipse(composition::Rect rect);

  void add_path(const Path& path);

  void close_subpath();

  auto elements() const noexcept -> std::span<const Element> {
    return elements_;
  }

  auto is_empty() const noexcept -> bool { return elements_.empty(); }

  auto current_point() const noexcept -> std::optional<composition::Point>;

  auto bounding_rect() const noexcept -> composition::Rect;

  auto contains(composition::Point point, bool eo_fill = false) const noexcept
      -> bool;

  auto applying(const AffineTransform& transform) const -> Path;

  auto trimmed_path(double from, double to) const -> Path;

  auto stroked_path(const StrokeStyle& style) const -> Path;

  auto normalized(bool eo_fill = true) const -> Path;

  auto union_with(const Path& other, bool eo_fill = false) const -> Path;

  auto intersection(const Path& other, bool eo_fill = false) const -> Path;

  auto subtracting(const Path& other, bool eo_fill = false) const -> Path;

  auto symmetric_difference(const Path& other, bool eo_fill = false) const
      -> Path;

  auto line_intersection(const Path& other, bool eo_fill = false) const -> Path;

  auto line_subtraction(const Path& other, bool eo_fill = false) const -> Path;

  friend auto operator==(const Path&, const Path&) -> bool = default;

 private:
  void continuous_corner(composition::Point corner, float radius_x,
                         float radius_y, float along_x, float along_y,
                         bool horizontal_first);

  void ensure_start(composition::Point at);

  void append_arc(composition::Point center, float radius, double begin,
                  double sweep, const AffineTransform& transform);

  static auto crossing(composition::Point from, composition::Point to,
                       composition::Point point) noexcept -> int;

  static constexpr int curve_steps = 16;

  std::vector<Element> elements_;
  std::optional<composition::Point> current_;
  std::optional<composition::Point> start_;
};

}
