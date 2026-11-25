#include <arisa/graphics/path.hpp>

#include <graphics/path_clipper.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <limits>
#include <numbers>
#include <type_traits>

namespace arisa::graphics {

Path::Path(composition::Rect rect) { add_rect(rect); }

Path::Path(composition::Rect rounded_rect, float corner_radius,
           theme::RoundedCornerStyle style) {
  add_rounded_rect(
      rounded_rect,
      composition::Size{.width = corner_radius, .height = corner_radius},
      style);
}

auto Path::ellipse_in(composition::Rect rect) -> Path {
  Path path;
  path.add_ellipse(rect);
  return path;
}

auto Path::shaped(theme::Shape shape, composition::Rect rect, float radius)
    -> Path {
  if (radius <= 0.0F) {
    return Path{rect};
  }
  return Path{rect, radius,
              shape.continuous ? theme::RoundedCornerStyle::continuous
                               : theme::RoundedCornerStyle::circular};
}

auto Path::shaped(theme::Shape shape, composition::Rect rect,
                  const theme::RectangleCornerRadii& radii) -> Path {
  if (shape.form == theme::ShapeForm::ellipse) {
    return ellipse_in(rect);
  }
  if (shape.form == theme::ShapeForm::circle) {
    const float side = std::min(rect.width, rect.height);
    return ellipse_in(
        composition::Rect{.x = rect.x + ((rect.width - side) * 0.5F),
                          .y = rect.y + ((rect.height - side) * 0.5F),
                          .width = side,
                          .height = side});
  }
  const theme::RoundedCornerStyle style =
      shape.continuous ? theme::RoundedCornerStyle::continuous
                       : theme::RoundedCornerStyle::circular;
  if (shape.elliptical()) {
    const float less =
        std::max(shape.corner_extent.width - radii.top_leading, 0.0F);
    Path path;
    path.add_rounded_rect(
        rect,
        composition::Size{
            .width = std::max(shape.corner_extent.width - less, 0.0F),
            .height = std::max(shape.corner_extent.height - less, 0.0F)},
        style);
    return path;
  }
  if (radii.top_leading == radii.bottom_leading &&
      radii.top_leading == radii.bottom_trailing &&
      radii.top_leading == radii.top_trailing) {
    return shaped(shape, rect, radii.top_leading);
  }
  Path path;
  path.add_rounded_rect(rect, radii, style);
  return path;
}

void Path::move(composition::Point to) {
  elements_.emplace_back(Move{to});
  current_ = to;
  start_ = to;
}

void Path::ensure_start(composition::Point at) {
  if (!current_.has_value()) {
    move(at);
  }
}

void Path::add_line(composition::Point to) {
  ensure_start(to);
  elements_.emplace_back(Line{to});
  current_ = to;
}

void Path::add_lines(std::span<const composition::Point> lines) {
  if (lines.empty()) {
    return;
  }
  move(lines.front());
  for (const composition::Point& point : lines.subspan(1)) {
    add_line(point);
  }
}

void Path::add_quad_curve(composition::Point to, composition::Point control) {
  ensure_start(control);
  elements_.emplace_back(QuadCurve{.to = to, .control = control});
  current_ = to;
}

void Path::add_curve(composition::Point to, composition::Point control1,
                     composition::Point control2) {
  ensure_start(control1);
  elements_.emplace_back(
      Curve{.to = to, .control1 = control1, .control2 = control2});
  current_ = to;
}

void Path::add_arc(composition::Point center, float radius,
                   motion::Angle start_angle, motion::Angle end_angle,
                   bool clockwise) {
  constexpr double tau = 2.0 * std::numbers::pi;
  const double begin = start_angle.radians();
  double sweep = end_angle.radians() - begin;
  if (clockwise) {
    if (sweep > 0.0) {
      sweep = std::fmod(sweep, tau) - tau;
    }
  } else if (sweep < 0.0) {
    sweep = std::fmod(sweep, tau) + tau;
  }
  append_arc(center, radius, begin, std::clamp(sweep, -tau, tau),
             AffineTransform{});
}

void Path::add_relative_arc(composition::Point center, float radius,
                            motion::Angle start_angle, motion::Angle delta,
                            const AffineTransform& transform) {
  append_arc(center, radius, start_angle.radians(), delta.radians(), transform);
}

void Path::add_arc(composition::Point tangent1_end,
                   composition::Point tangent2_end, float radius,
                   const AffineTransform& transform) {
  const composition::Point corner = transform.apply(tangent1_end);
  if (!current_.has_value()) {
    move(corner);
    return;
  }
  const float a = transform.a();
  const float b = transform.b();
  const float c = transform.c();
  const float d = transform.d();
  const float determinant = (a * d) - (b * c);
  if (determinant == 0.0F) {
    add_line(corner);
    return;
  }
  const composition::Point here{.x = ((d * (current_->x - transform.tx())) -
                                      (c * (current_->y - transform.ty()))) /
                                     determinant,
                                .y = ((a * (current_->y - transform.ty())) -
                                      (b * (current_->x - transform.tx()))) /
                                     determinant};
  const double ux = static_cast<double>(here.x) - tangent1_end.x;
  const double uy = static_cast<double>(here.y) - tangent1_end.y;
  const double vx = static_cast<double>(tangent2_end.x) - tangent1_end.x;
  const double vy = static_cast<double>(tangent2_end.y) - tangent1_end.y;
  const double u_length = std::hypot(ux, uy);
  const double v_length = std::hypot(vx, vy);
  const double cross = (ux * vy) - (uy * vx);
  if (radius <= 0.0F || u_length == 0.0 || v_length == 0.0 ||
      std::abs(cross) < 1.0e-9 * u_length * v_length) {
    add_line(corner);
    return;
  }
  const double cosine =
      std::clamp(((ux * vx) + (uy * vy)) / (u_length * v_length), -1.0, 1.0);
  const double half = std::acos(cosine) * 0.5;
  const double reach = radius / std::tan(half);
  const double centre_reach = radius / std::sin(half);
  const double bx = (ux / u_length) + (vx / v_length);
  const double by = (uy / u_length) + (vy / v_length);
  const double b_length = std::hypot(bx, by);
  const composition::Point centre{
      .x = static_cast<float>(tangent1_end.x + (bx / b_length * centre_reach)),
      .y = static_cast<float>(tangent1_end.y + (by / b_length * centre_reach))};
  const double start_x = tangent1_end.x + (ux / u_length * reach);
  const double start_y = tangent1_end.y + (uy / u_length * reach);
  const double end_x = tangent1_end.x + (vx / v_length * reach);
  const double end_y = tangent1_end.y + (vy / v_length * reach);
  const double begin = std::atan2(start_y - centre.y, start_x - centre.x);
  double sweep = std::atan2(end_y - centre.y, end_x - centre.x) - begin;
  if (sweep > std::numbers::pi) {
    sweep -= 2.0 * std::numbers::pi;
  } else if (sweep < -std::numbers::pi) {
    sweep += 2.0 * std::numbers::pi;
  }
  append_arc(centre, radius, begin, sweep, transform);
}

void Path::append_arc(composition::Point center, float radius, double begin,
                      double sweep, const AffineTransform& transform) {
  const auto point_at = [&](double angle) {
    return composition::Point{
        .x = center.x + (radius * static_cast<float>(std::cos(angle))),
        .y = center.y + (radius * static_cast<float>(std::sin(angle)))};
  };
  const composition::Point first = transform.apply(point_at(begin));
  if (current_.has_value()) {
    add_line(first);
  } else {
    move(first);
  }
  const int segments = std::max(
      1, static_cast<int>(
             std::ceil(std::abs(sweep) / (std::numbers::pi / 2.0) - 1.0e-6)));
  const double step = sweep / segments;
  const double handle = 4.0 / 3.0 * std::tan(step / 4.0) * radius;
  double angle = begin;
  for (int segment = 0; segment < segments; ++segment) {
    const double next = angle + step;
    const composition::Point from = point_at(angle);
    const composition::Point to = point_at(next);
    add_curve(transform.apply(to),
              transform.apply(composition::Point{
                  .x = from.x - static_cast<float>(handle * std::sin(angle)),
                  .y = from.y + static_cast<float>(handle * std::cos(angle))}),
              transform.apply(composition::Point{
                  .x = to.x + static_cast<float>(handle * std::sin(next)),
                  .y = to.y - static_cast<float>(handle * std::cos(next))}));
    angle = next;
  }
}

void Path::add_rects(std::span<const composition::Rect> rects,
                     const AffineTransform& transform) {
  for (const composition::Rect& rect : rects) {
    add_path(Path{rect}.applying(transform));
  }
}

void Path::add_rect(composition::Rect rect) {
  move({rect.x, rect.y});
  add_line({rect.x + rect.width, rect.y});
  add_line({rect.x + rect.width, rect.y + rect.height});
  add_line({rect.x, rect.y + rect.height});
  close_subpath();
}

void Path::continuous_corner(composition::Point corner, float radius_x,
                             float radius_y, float along_x, float along_y,
                             bool horizontal_first) {
  struct Step {
    float a;
    float b;
  };
  constexpr std::array<Step, 9> curve{{{1.08849323F, 0.0F},
                                       {0.86840689F, 0.02261690F},
                                       {0.66993427F, 0.06549600F},
                                       {0.37282392F, 0.16905899F},
                                       {0.16905899F, 0.37282392F},
                                       {0.06549600F, 0.66993427F},
                                       {0.02261690F, 0.86840689F},
                                       {0.0F, 1.08849323F},
                                       {0.0F, continuous_reach}}};
  const auto at = [&](Step step) {
    const float first = horizontal_first ? step.a : step.b;
    const float second = horizontal_first ? step.b : step.a;
    return composition::Point{.x = corner.x + (along_x * first * radius_x),
                              .y = corner.y + (along_y * second * radius_y)};
  };
  add_curve(at(curve[2]), at(curve[0]), at(curve[1]));
  add_line(at(Step{0.63149399F, 0.07491100F}));
  add_curve(at(Step{0.07491100F, 0.63149399F}), at(curve[3]), at(curve[4]));
  add_line(at(curve[5]));
  add_curve(at(curve[8]), at(curve[6]), at(curve[7]));
}

void Path::add_rounded_rect(composition::Rect rect,
                            composition::Size corner_size,
                            theme::RoundedCornerStyle style) {
  const float half_width = rect.width * 0.5F;
  const float half_height = rect.height * 0.5F;
  float radius_x = std::clamp(corner_size.width, 0.0F, half_width);
  float radius_y = std::clamp(corner_size.height, 0.0F, half_height);
  if (radius_x <= 0.0F || radius_y <= 0.0F) {
    add_rect(rect);
    return;
  }
  const float left = rect.x;
  const float top = rect.y;
  const float right = rect.x + rect.width;
  const float bottom = rect.y + rect.height;
  if (style == theme::RoundedCornerStyle::continuous) {
    radius_x = std::min(radius_x, half_width / continuous_reach);
    radius_y = std::min(radius_y, half_height / continuous_reach);
    const float reach_x = radius_x * continuous_reach;
    const float reach_y = radius_y * continuous_reach;
    move({left + reach_x, top});
    add_line({right - reach_x, top});
    continuous_corner({right, top}, radius_x, radius_y, -1.0F, 1.0F, true);
    add_line({right, bottom - reach_y});
    continuous_corner({right, bottom}, radius_x, radius_y, -1.0F, -1.0F, false);
    add_line({left + reach_x, bottom});
    continuous_corner({left, bottom}, radius_x, radius_y, 1.0F, -1.0F, true);
    add_line({left, top + reach_y});
    continuous_corner({left, top}, radius_x, radius_y, 1.0F, 1.0F, false);
    close_subpath();
    return;
  }
  constexpr float kappa = 0.5522847498F;
  const float handle_x = radius_x * kappa;
  const float handle_y = radius_y * kappa;
  move({left + radius_x, top});
  add_line({right - radius_x, top});
  add_curve({right, top + radius_y}, {right - radius_x + handle_x, top},
            {right, top + radius_y - handle_y});
  add_line({right, bottom - radius_y});
  add_curve({right - radius_x, bottom}, {right, bottom - radius_y + handle_y},
            {right - radius_x + handle_x, bottom});
  add_line({left + radius_x, bottom});
  add_curve({left, bottom - radius_y}, {left + radius_x - handle_x, bottom},
            {left, bottom - radius_y + handle_y});
  add_line({left, top + radius_y});
  add_curve({left + radius_x, top}, {left, top + radius_y - handle_y},
            {left + radius_x - handle_x, top});
  close_subpath();
}

void Path::add_rounded_rect(composition::Rect rect,
                            const theme::RectangleCornerRadii& radii,
                            theme::RoundedCornerStyle style) {
  const bool continuous = style == theme::RoundedCornerStyle::continuous;
  const float limit = std::min(rect.width, rect.height) * 0.5F /
                      (continuous ? continuous_reach : 1.0F);
  const auto clamp = [limit](float radius) {
    return std::clamp(radius, 0.0F, std::max(limit, 0.0F));
  };
  const float top_left = clamp(radii.top_leading);
  const float top_right = clamp(radii.top_trailing);
  const float bottom_right = clamp(radii.bottom_trailing);
  const float bottom_left = clamp(radii.bottom_leading);
  const float reach = continuous ? continuous_reach : 1.0F;
  const float left = rect.x;
  const float top = rect.y;
  const float right = rect.x + rect.width;
  const float bottom = rect.y + rect.height;
  constexpr float kappa = 0.5522847498F;
  const auto corner = [&](composition::Point at, float radius, float along_x,
                          float along_y, bool horizontal_first) {
    if (radius <= 0.0F) {
      add_line(at);
      return;
    }
    if (continuous) {
      continuous_corner(at, radius, radius, along_x, along_y, horizontal_first);
      return;
    }
    const float handle = radius * (1.0F - kappa);
    if (horizontal_first) {
      add_curve({at.x, at.y + (along_y * radius)},
                {at.x + (along_x * handle), at.y},
                {at.x, at.y + (along_y * handle)});
    } else {
      add_curve({at.x + (along_x * radius), at.y},
                {at.x, at.y + (along_y * handle)},
                {at.x + (along_x * handle), at.y});
    }
  };
  move({left + (top_left * reach), top});
  add_line({right - (top_right * reach), top});
  corner({right, top}, top_right, -1.0F, 1.0F, true);
  add_line({right, bottom - (bottom_right * reach)});
  corner({right, bottom}, bottom_right, -1.0F, -1.0F, false);
  add_line({left + (bottom_left * reach), bottom});
  corner({left, bottom}, bottom_left, 1.0F, -1.0F, true);
  add_line({left, top + (top_left * reach)});
  corner({left, top}, top_left, 1.0F, 1.0F, false);
  close_subpath();
}

void Path::add_ellipse(composition::Rect rect) {
  constexpr float kappa = 0.5522847498F;
  const float radius_x = rect.width * 0.5F;
  const float radius_y = rect.height * 0.5F;
  const float centre_x = rect.x + radius_x;
  const float centre_y = rect.y + radius_y;
  const float handle_x = radius_x * kappa;
  const float handle_y = radius_y * kappa;
  move({centre_x + radius_x, centre_y});
  add_curve({centre_x, centre_y + radius_y},
            {centre_x + radius_x, centre_y + handle_y},
            {centre_x + handle_x, centre_y + radius_y});
  add_curve({centre_x - radius_x, centre_y},
            {centre_x - handle_x, centre_y + radius_y},
            {centre_x - radius_x, centre_y + handle_y});
  add_curve({centre_x, centre_y - radius_y},
            {centre_x - radius_x, centre_y - handle_y},
            {centre_x - handle_x, centre_y - radius_y});
  add_curve({centre_x + radius_x, centre_y},
            {centre_x + handle_x, centre_y - radius_y},
            {centre_x + radius_x, centre_y - handle_y});
  close_subpath();
}

void Path::add_path(const Path& path) {
  elements_.insert(elements_.end(), path.elements_.begin(),
                   path.elements_.end());
  if (path.current_.has_value()) {
    current_ = path.current_;
    start_ = path.start_;
  }
}

void Path::close_subpath() {
  if (!current_.has_value()) {
    return;
  }
  elements_.emplace_back(CloseSubpath{});
  current_ = start_;
}

auto Path::current_point() const noexcept -> std::optional<composition::Point> {
  return current_;
}

auto Path::applying(const AffineTransform& transform) const -> Path {
  Path moved;
  moved.elements_.reserve(elements_.size());
  for (const Element& element : elements_) {
    std::visit(
        [&moved, &transform](const auto& part) {
          using Part = std::decay_t<decltype(part)>;
          if constexpr (std::is_same_v<Part, Move>) {
            moved.elements_.emplace_back(Move{.to = transform.apply(part.to)});
          } else if constexpr (std::is_same_v<Part, Line>) {
            moved.elements_.emplace_back(Line{.to = transform.apply(part.to)});
          } else if constexpr (std::is_same_v<Part, QuadCurve>) {
            moved.elements_.emplace_back(
                QuadCurve{.to = transform.apply(part.to),
                          .control = transform.apply(part.control)});
          } else if constexpr (std::is_same_v<Part, Curve>) {
            moved.elements_.emplace_back(
                Curve{.to = transform.apply(part.to),
                      .control1 = transform.apply(part.control1),
                      .control2 = transform.apply(part.control2)});
          } else {
            moved.elements_.emplace_back(part);
          }
        },
        element);
  }
  return moved;
}

auto Path::bounding_rect() const noexcept -> composition::Rect {
  float left = std::numeric_limits<float>::max();
  float top = std::numeric_limits<float>::max();
  float right = std::numeric_limits<float>::lowest();
  float bottom = std::numeric_limits<float>::lowest();
  const auto include = [&](composition::Point point) {
    left = std::min(left, point.x);
    top = std::min(top, point.y);
    right = std::max(right, point.x);
    bottom = std::max(bottom, point.y);
  };
  for (const Element& element : elements_) {
    std::visit(
        [&](const auto& each) {
          using T = std::remove_cvref_t<decltype(each)>;
          if constexpr (std::same_as<T, Curve>) {
            include(each.control1);
            include(each.control2);
            include(each.to);
          } else if constexpr (std::same_as<T, QuadCurve>) {
            include(each.control);
            include(each.to);
          } else if constexpr (!std::same_as<T, CloseSubpath>) {
            include(each.to);
          }
        },
        element);
  }
  if (left > right) {
    return composition::Rect{};
  }
  return composition::Rect{
      .x = left, .y = top, .width = right - left, .height = bottom - top};
}

auto Path::crossing(composition::Point from, composition::Point to,
                    composition::Point point) noexcept -> int {
  if ((from.y <= point.y) == (to.y <= point.y)) {
    return 0;
  }
  const float across =
      from.x + ((point.y - from.y) * (to.x - from.x) / (to.y - from.y));
  if (across <= point.x) {
    return 0;
  }
  return to.y > from.y ? 1 : -1;
}

auto Path::contains(composition::Point point, bool eo_fill) const noexcept
    -> bool {
  int winding = 0;
  composition::Point start{};
  composition::Point at{};
  bool open = false;
  const auto close = [&] {
    if (open) {
      winding += crossing(at, start, point);
    }
    at = start;
    open = false;
  };
  for (const Element& element : elements_) {
    std::visit(
        [&](const auto& each) {
          using T = std::remove_cvref_t<decltype(each)>;
          if constexpr (std::same_as<T, Move>) {
            close();
            start = each.to;
            at = each.to;
          } else if constexpr (std::same_as<T, Line>) {
            winding += crossing(at, each.to, point);
            at = each.to;
            open = true;
          } else if constexpr (std::same_as<T, QuadCurve>) {
            composition::Point previous = at;
            for (int step = 1; step <= curve_steps; ++step) {
              const float t =
                  static_cast<float>(step) / static_cast<float>(curve_steps);
              const float u = 1.0F - t;
              const composition::Point next{
                  .x = (u * u * at.x) + (2.0F * u * t * each.control.x) +
                       (t * t * each.to.x),
                  .y = (u * u * at.y) + (2.0F * u * t * each.control.y) +
                       (t * t * each.to.y)};
              winding += crossing(previous, next, point);
              previous = next;
            }
            at = each.to;
            open = true;
          } else if constexpr (std::same_as<T, Curve>) {
            composition::Point previous = at;
            for (int step = 1; step <= curve_steps; ++step) {
              const float t =
                  static_cast<float>(step) / static_cast<float>(curve_steps);
              const float u = 1.0F - t;
              const float a = u * u * u;
              const float b = 3.0F * u * u * t;
              const float c = 3.0F * u * t * t;
              const float d = t * t * t;
              const composition::Point next{
                  .x = (a * at.x) + (b * each.control1.x) +
                       (c * each.control2.x) + (d * each.to.x),
                  .y = (a * at.y) + (b * each.control1.y) +
                       (c * each.control2.y) + (d * each.to.y)};
              winding += crossing(previous, next, point);
              previous = next;
            }
            at = each.to;
            open = true;
          } else {
            close();
          }
        },
        element);
  }
  close();
  return eo_fill ? (winding % 2) != 0 : winding != 0;
}

auto Path::trimmed_path(double from, double to) const -> Path {
  return PathClipper::trimmed(*this, from, to);
}

auto Path::stroked_path(const StrokeStyle& style) const -> Path {
  return PathClipper::outline(*this, style);
}

auto Path::normalized(bool eo_fill) const -> Path {
  return PathClipper::normalized(*this, eo_fill);
}

auto Path::union_with(const Path& other, bool eo_fill) const -> Path {
  return PathClipper::combine(*this, other, eo_fill,
                              PathClipper::Operation::unite);
}

auto Path::intersection(const Path& other, bool eo_fill) const -> Path {
  return PathClipper::combine(*this, other, eo_fill,
                              PathClipper::Operation::intersect);
}

auto Path::subtracting(const Path& other, bool eo_fill) const -> Path {
  return PathClipper::combine(*this, other, eo_fill,
                              PathClipper::Operation::subtract);
}

auto Path::symmetric_difference(const Path& other, bool eo_fill) const -> Path {
  return PathClipper::combine(*this, other, eo_fill,
                              PathClipper::Operation::exclude);
}

auto Path::line_intersection(const Path& other, bool eo_fill) const -> Path {
  return PathClipper::clipped_lines(*this, other, eo_fill, true);
}

auto Path::line_subtraction(const Path& other, bool eo_fill) const -> Path {
  return PathClipper::clipped_lines(*this, other, eo_fill, false);
}

}
