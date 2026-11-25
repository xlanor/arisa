#include <graphics/path_clipper.hpp>

#include <arisa/support/unique_handle.hpp>

#include <tesselator.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <type_traits>
#include <utility>
#include <variant>

namespace arisa::graphics {

auto PathClipper::lerp(composition::Point a, composition::Point b, double t)
    -> composition::Point {
  return composition::Point{.x = static_cast<float>(a.x + ((b.x - a.x) * t)),
                            .y = static_cast<float>(a.y + ((b.y - a.y) * t))};
}

auto PathClipper::distance(composition::Point a, composition::Point b)
    -> double {
  return std::hypot(static_cast<double>(b.x) - a.x,
                    static_cast<double>(b.y) - a.y);
}

void PathClipper::flatten_quad(composition::Point from,
                               composition::Point control,
                               composition::Point to,
                               std::vector<composition::Point>& into) {
  const double reach = distance(from, control) + distance(control, to);
  const int steps =
      std::clamp(static_cast<int>(std::ceil(reach / flatness)), 2, 256);
  for (int step = 1; step <= steps; ++step) {
    const double t = static_cast<double>(step) / steps;
    into.push_back(lerp(lerp(from, control, t), lerp(control, to, t), t));
  }
}

void PathClipper::flatten_cubic(composition::Point from,
                                composition::Point first,
                                composition::Point second,
                                composition::Point to,
                                std::vector<composition::Point>& into) {
  const double reach =
      distance(from, first) + distance(first, second) + distance(second, to);
  const int steps =
      std::clamp(static_cast<int>(std::ceil(reach / flatness)), 2, 512);
  for (int step = 1; step <= steps; ++step) {
    const double t = static_cast<double>(step) / steps;
    const composition::Point a = lerp(from, first, t);
    const composition::Point b = lerp(first, second, t);
    const composition::Point c = lerp(second, to, t);
    into.push_back(lerp(lerp(a, b, t), lerp(b, c, t), t));
  }
}

auto PathClipper::polylines(const Path& path) -> std::vector<Polyline> {
  std::vector<Polyline> lines;
  composition::Point at{};
  composition::Point start{};
  const auto open = [&lines, &at] {
    if (lines.empty() || lines.back().closed || lines.back().points.empty()) {
      lines.push_back(Polyline{.points = {at}, .closed = false});
    }
  };
  for (const Path::Element& element : path.elements()) {
    std::visit(
        [&](const auto& each) {
          using T = std::remove_cvref_t<decltype(each)>;
          if constexpr (std::same_as<T, Path::Move>) {
            at = each.to;
            start = each.to;
            lines.push_back(Polyline{.points = {at}, .closed = false});
          } else if constexpr (std::same_as<T, Path::Line>) {
            open();
            lines.back().points.push_back(each.to);
            at = each.to;
          } else if constexpr (std::same_as<T, Path::QuadCurve>) {
            open();
            flatten_quad(at, each.control, each.to, lines.back().points);
            at = each.to;
          } else if constexpr (std::same_as<T, Path::Curve>) {
            open();
            flatten_cubic(at, each.control1, each.control2, each.to,
                          lines.back().points);
            at = each.to;
          } else {
            if (!lines.empty() && !lines.back().closed) {
              lines.back().closed = true;
            }
            at = start;
          }
        },
        element);
  }
  for (Polyline& line : lines) {
    if (line.closed && line.points.size() > 1 &&
        line.points.front() == line.points.back()) {
      line.points.pop_back();
    }
  }
  std::erase_if(lines,
                [](const Polyline& line) { return line.points.empty(); });
  return lines;
}

auto PathClipper::signed_area(const Polygon& polygon) -> double {
  double area = 0.0;
  for (std::size_t index = 0; index < polygon.size(); ++index) {
    const composition::Point& a = polygon[index];
    const composition::Point& b = polygon[(index + 1) % polygon.size()];
    area += (static_cast<double>(a.x) * b.y) - (static_cast<double>(b.x) * a.y);
  }
  return area * 0.5;
}

auto PathClipper::orientation(std::span<const Polygon> polygons) -> double {
  double total = 0.0;
  for (const Polygon& polygon : polygons) {
    total += signed_area(polygon);
  }
  return total;
}

auto PathClipper::reversed(std::span<const Polygon> polygons)
    -> std::vector<Polygon> {
  std::vector<Polygon> flipped(polygons.begin(), polygons.end());
  for (Polygon& polygon : flipped) {
    std::ranges::reverse(polygon);
  }
  return flipped;
}

auto PathClipper::boundary(std::span<const Polygon> polygons, Rule rule)
    -> std::vector<Polygon> {
  struct TessTraits {
    using handle_type = TESStesselator*;

    static auto empty() noexcept -> handle_type { return nullptr; }

    static auto is_valid(const handle_type& handle) noexcept -> bool {
      return handle != nullptr;
    }

    static void release(handle_type& handle) noexcept {
      tessDeleteTess(handle);
    }
  };
  std::vector<Polygon> found;
  const support::UniqueHandle<TessTraits> tess{tessNewTess(nullptr)};
  if (!tess.valid()) {
    return found;
  }
  bool any = false;
  for (const Polygon& polygon : polygons) {
    if (polygon.size() < 3) {
      continue;
    }
    tessAddContour(tess.get(), 2, polygon.data(),
                   static_cast<int>(sizeof(composition::Point)),
                   static_cast<int>(polygon.size()));
    any = true;
  }
  if (!any) {
    return found;
  }
  int winding = TESS_WINDING_NONZERO;
  switch (rule) {
    case Rule::odd:
      winding = TESS_WINDING_ODD;
      break;
    case Rule::nonzero:
      winding = TESS_WINDING_NONZERO;
      break;
    case Rule::positive:
      winding = TESS_WINDING_POSITIVE;
      break;
    case Rule::negative:
      winding = TESS_WINDING_NEGATIVE;
      break;
    case Rule::two:
      winding = TESS_WINDING_ABS_GEQ_TWO;
      break;
  }
  const std::array<TESSreal, 3> normal{0.0F, 0.0F, 1.0F};
  if (tessTesselate(tess.get(), winding, TESS_BOUNDARY_CONTOURS, 3, 2,
                    normal.data()) == 0) {
    return found;
  }
  const TESSreal* vertices = tessGetVertices(tess.get());
  const TESSindex* elements = tessGetElements(tess.get());
  const int count = tessGetElementCount(tess.get());
  for (int index = 0; index < count; ++index) {
    const TESSindex first = elements[index * 2];
    const TESSindex size = elements[(index * 2) + 1];
    Polygon polygon;
    polygon.reserve(static_cast<std::size_t>(size));
    for (TESSindex point = 0; point < size; ++point) {
      const auto at = static_cast<std::size_t>(first + point) * 2;
      polygon.push_back(
          composition::Point{.x = vertices[at], .y = vertices[at + 1]});
    }
    if (polygon.size() >= 3) {
      found.push_back(std::move(polygon));
    }
  }
  return found;
}

auto PathClipper::regions(const Path& path, bool eo_fill)
    -> std::vector<Polygon> {
  std::vector<Polygon> polygons;
  for (Polyline& line : polylines(path)) {
    polygons.push_back(std::move(line.points));
  }
  return boundary(polygons, eo_fill ? Rule::odd : Rule::nonzero);
}

auto PathClipper::path_of(std::span<const Polygon> polygons) -> Path {
  Path made;
  for (const Polygon& polygon : polygons) {
    if (polygon.size() < 3) {
      continue;
    }
    made.add_lines(polygon);
    made.close_subpath();
  }
  return made;
}

auto PathClipper::normalized(const Path& path, bool eo_fill) -> Path {
  return path_of(regions(path, eo_fill));
}

auto PathClipper::combine(const Path& first, const Path& second, bool eo_fill,
                          Operation operation) -> Path {
  const std::vector<Polygon> a = regions(first, eo_fill);
  std::vector<Polygon> b = regions(second, eo_fill);
  const double sign = orientation(a);
  if (sign * orientation(b) < 0.0) {
    b = reversed(b);
  }
  const double reference = sign != 0.0 ? sign : orientation(b);
  std::vector<Polygon> all = a;
  switch (operation) {
    case Operation::unite:
      all.insert(all.end(), b.begin(), b.end());
      return path_of(boundary(all, Rule::nonzero));
    case Operation::intersect:
      all.insert(all.end(), b.begin(), b.end());
      return path_of(boundary(all, Rule::two));
    case Operation::subtract: {
      if (a.empty()) {
        return Path{};
      }
      const std::vector<Polygon> flipped = reversed(b);
      all.insert(all.end(), flipped.begin(), flipped.end());
      return path_of(
          boundary(all, reference > 0.0 ? Rule::positive : Rule::negative));
    }
    case Operation::exclude:
      all.insert(all.end(), b.begin(), b.end());
      return path_of(boundary(all, Rule::odd));
  }
  return Path{};
}

auto PathClipper::disc(composition::Point centre, float radius) -> Polygon {
  Polygon polygon;
  polygon.reserve(arc_steps);
  for (int step = 0; step < arc_steps; ++step) {
    const double angle = 2.0 * std::numbers::pi * step / arc_steps;
    polygon.push_back(composition::Point{
        .x = centre.x + static_cast<float>(radius * std::cos(angle)),
        .y = centre.y + static_cast<float>(radius * std::sin(angle))});
  }
  return polygon;
}

void PathClipper::join(composition::Point at, composition::Point before,
                       composition::Point after, const StrokeStyle& style,
                       std::vector<Polygon>& into) {
  const float half = style.line_width * 0.5F;
  if (style.line_join == LineJoin::round) {
    into.push_back(disc(at, half));
    return;
  }
  const double in_length = distance(before, at);
  const double out_length = distance(at, after);
  if (in_length <= 0.0 || out_length <= 0.0) {
    return;
  }
  const double ix = (at.x - before.x) / in_length;
  const double iy = (at.y - before.y) / in_length;
  const double ox = (after.x - at.x) / out_length;
  const double oy = (after.y - at.y) / out_length;
  const double turn = (ix * oy) - (iy * ox);
  if (std::abs(turn) < 1.0e-9) {
    return;
  }
  const double side = turn > 0.0 ? -1.0 : 1.0;
  const composition::Point outer_in{
      .x = at.x + static_cast<float>(-iy * half * side),
      .y = at.y + static_cast<float>(ix * half * side)};
  const composition::Point outer_out{
      .x = at.x + static_cast<float>(-oy * half * side),
      .y = at.y + static_cast<float>(ox * half * side)};
  const double cosine = std::clamp((ix * ox) + (iy * oy), -1.0, 1.0);
  const double theta = std::acos(cosine);
  const double miter = 1.0 / std::max(std::cos(theta * 0.5), 1.0e-9);
  if (style.line_join == LineJoin::miter && miter <= style.miter_limit) {
    const double mx = (-iy - oy) * side;
    const double my = (ix + ox) * side;
    const double length = std::hypot(mx, my);
    if (length > 1.0e-9) {
      const double reach = half * miter;
      const composition::Point tip{
          .x = at.x + static_cast<float>(mx / length * reach),
          .y = at.y + static_cast<float>(my / length * reach)};
      into.push_back(Polygon{at, outer_in, tip, outer_out});
      return;
    }
  }
  into.push_back(Polygon{at, outer_in, outer_out});
}

void PathClipper::cap(composition::Point at, composition::Point toward,
                      const StrokeStyle& style, std::vector<Polygon>& into) {
  const float half = style.line_width * 0.5F;
  if (style.line_cap == LineCap::round) {
    into.push_back(disc(at, half));
    return;
  }
  if (style.line_cap != LineCap::square) {
    return;
  }
  const double length = distance(at, toward);
  if (length <= 0.0) {
    return;
  }
  const double dx = (at.x - toward.x) / length;
  const double dy = (at.y - toward.y) / length;
  const auto offset = [&](double along, double across) {
    return composition::Point{
        .x = at.x + static_cast<float>((dx * along) - (dy * across)),
        .y = at.y + static_cast<float>((dy * along) + (dx * across))};
  };
  into.push_back(Polygon{offset(0.0, -half), offset(half, -half),
                         offset(half, half), offset(0.0, half)});
}

void PathClipper::stroke_line(const Polyline& line, const StrokeStyle& style,
                              std::vector<Polygon>& into) {
  const float half = style.line_width * 0.5F;
  std::vector<composition::Point> points;
  for (const composition::Point& point : line.points) {
    if (points.empty() || distance(points.back(), point) > 1.0e-6) {
      points.push_back(point);
    }
  }
  if (line.closed && points.size() > 1 &&
      distance(points.front(), points.back()) <= 1.0e-6) {
    points.pop_back();
  }
  if (points.size() == 1) {
    if (style.line_cap == LineCap::round) {
      into.push_back(disc(points.front(), half));
    } else if (style.line_cap == LineCap::square) {
      const composition::Point& at = points.front();
      into.push_back(Polygon{{at.x - half, at.y - half},
                             {at.x + half, at.y - half},
                             {at.x + half, at.y + half},
                             {at.x - half, at.y + half}});
    }
    return;
  }
  const std::size_t segments = line.closed ? points.size() : points.size() - 1;
  for (std::size_t index = 0; index < segments; ++index) {
    const composition::Point& a = points[index];
    const composition::Point& b = points[(index + 1) % points.size()];
    const double length = distance(a, b);
    const auto nx = static_cast<float>(-(b.y - a.y) / length * half);
    const auto ny = static_cast<float>((b.x - a.x) / length * half);
    into.push_back(Polygon{{a.x + nx, a.y + ny},
                           {b.x + nx, b.y + ny},
                           {b.x - nx, b.y - ny},
                           {a.x - nx, a.y - ny}});
  }
  const std::size_t count = points.size();
  if (line.closed) {
    for (std::size_t index = 0; index < count; ++index) {
      join(points[index], points[(index + count - 1) % count],
           points[(index + 1) % count], style, into);
    }
    return;
  }
  for (std::size_t index = 1; index + 1 < count; ++index) {
    join(points[index], points[index - 1], points[index + 1], style, into);
  }
  cap(points.front(), points[1], style, into);
  cap(points.back(), points[count - 2], style, into);
}

auto PathClipper::dashed(std::span<const Polyline> lines,
                         std::span<const float> pattern, float phase)
    -> std::vector<Polyline> {
  double period = 0.0;
  for (const float each : pattern) {
    period += std::max(each, 0.0F);
  }
  if (period <= 0.0) {
    return {lines.begin(), lines.end()};
  }
  std::vector<Polyline> dashes;
  for (const Polyline& line : lines) {
    std::vector<composition::Point> points = line.points;
    if (line.closed && !points.empty()) {
      points.push_back(points.front());
    }
    std::size_t slot = 0;
    double left = std::fmod(static_cast<double>(phase), period);
    if (left < 0.0) {
      left += period;
    }
    while (left >= std::max(pattern[slot], 0.0F) && left > 0.0) {
      left -= std::max(pattern[slot], 0.0F);
      slot = (slot + 1) % pattern.size();
    }
    double remaining = std::max(pattern[slot], 0.0F) - left;
    bool on = slot % 2 == 0;
    Polyline current{.points = {}, .closed = false};
    if (on && !points.empty()) {
      current.points.push_back(points.front());
    }
    for (std::size_t index = 1; index < points.size(); ++index) {
      composition::Point from = points[index - 1];
      const composition::Point to = points[index];
      double length = distance(from, to);
      while (length > remaining) {
        const composition::Point cut = lerp(from, to, remaining / length);
        if (on) {
          current.points.push_back(cut);
          dashes.push_back(std::move(current));
          current = Polyline{.points = {}, .closed = false};
        } else {
          current.points.push_back(cut);
        }
        length -= remaining;
        from = cut;
        slot = (slot + 1) % pattern.size();
        remaining = std::max(pattern[slot], 0.0F);
        on = slot % 2 == 0;
      }
      remaining -= length;
      if (on) {
        current.points.push_back(to);
      }
    }
    if (on && current.points.size() > 1) {
      dashes.push_back(std::move(current));
    }
  }
  return dashes;
}

auto PathClipper::outline(const Path& path, const StrokeStyle& style) -> Path {
  if (style.line_width <= 0.0F) {
    return Path{};
  }
  std::vector<Polyline> lines = polylines(path);
  if (!style.dash.empty()) {
    lines = dashed(lines, style.dash, style.dash_phase);
  }
  std::vector<Polygon> pieces;
  for (const Polyline& line : lines) {
    stroke_line(line, style, pieces);
  }
  for (Polygon& polygon : pieces) {
    if (signed_area(polygon) < 0.0) {
      std::ranges::reverse(polygon);
    }
  }
  return path_of(boundary(pieces, Rule::nonzero));
}

auto PathClipper::clipped_lines(const Path& lines, const Path& region,
                                bool eo_fill, bool inside) -> Path {
  std::vector<std::pair<composition::Point, composition::Point>> edges;
  for (const Polyline& line : polylines(region)) {
    const std::size_t count = line.points.size();
    for (std::size_t index = 0; index < count; ++index) {
      edges.emplace_back(line.points[index], line.points[(index + 1) % count]);
    }
  }
  Path made;
  for (const Polyline& line : polylines(lines)) {
    std::vector<composition::Point> points = line.points;
    if (line.closed && !points.empty()) {
      points.push_back(points.front());
    }
    bool drawing = false;
    for (std::size_t index = 1; index < points.size(); ++index) {
      const composition::Point a = points[index - 1];
      const composition::Point b = points[index];
      std::vector<double> cuts{0.0, 1.0};
      for (const auto& [c, d] : edges) {
        const double rx = static_cast<double>(b.x) - a.x;
        const double ry = static_cast<double>(b.y) - a.y;
        const double sx = static_cast<double>(d.x) - c.x;
        const double sy = static_cast<double>(d.y) - c.y;
        const double denominator = (rx * sy) - (ry * sx);
        if (std::abs(denominator) < 1.0e-12) {
          continue;
        }
        const double qx = static_cast<double>(c.x) - a.x;
        const double qy = static_cast<double>(c.y) - a.y;
        const double t = ((qx * sy) - (qy * sx)) / denominator;
        const double u = ((qx * ry) - (qy * rx)) / denominator;
        if (t > 0.0 && t < 1.0 && u >= 0.0 && u <= 1.0) {
          cuts.push_back(t);
        }
      }
      std::ranges::sort(cuts);
      for (std::size_t cut = 1; cut < cuts.size(); ++cut) {
        const double from = cuts[cut - 1];
        const double to = cuts[cut];
        if (to - from < 1.0e-9) {
          continue;
        }
        const bool within =
            region.contains(lerp(a, b, (from + to) * 0.5), eo_fill);
        if (within == inside) {
          if (!drawing) {
            made.move(lerp(a, b, from));
            drawing = true;
          }
          made.add_line(lerp(a, b, to));
        } else {
          drawing = false;
        }
      }
    }
  }
  return made;
}

auto PathClipper::end_of(const Path::Element& element, composition::Point start)
    -> composition::Point {
  return std::visit(
      [start](const auto& each) -> composition::Point {
        using T = std::remove_cvref_t<decltype(each)>;
        if constexpr (std::same_as<T, Path::CloseSubpath>) {
          return start;
        } else {
          return each.to;
        }
      },
      element);
}

auto PathClipper::point_at(const Piece& piece, double t) -> composition::Point {
  return std::visit(
      [&piece, t](const auto& each) -> composition::Point {
        using T = std::remove_cvref_t<decltype(each)>;
        if constexpr (std::same_as<T, Path::QuadCurve>) {
          return lerp(lerp(piece.start, each.control, t),
                      lerp(each.control, each.to, t), t);
        } else if constexpr (std::same_as<T, Path::Curve>) {
          const composition::Point a = lerp(piece.start, each.control1, t);
          const composition::Point b = lerp(each.control1, each.control2, t);
          const composition::Point c = lerp(each.control2, each.to, t);
          return lerp(lerp(a, b, t), lerp(b, c, t), t);
        } else if constexpr (std::same_as<T, Path::Line>) {
          return lerp(piece.start, each.to, t);
        } else {
          return piece.start;
        }
      },
      piece.element);
}

auto PathClipper::pieces(const Path& path) -> std::vector<Piece> {
  std::vector<Piece> found;
  composition::Point at{};
  composition::Point start{};
  bool begins = true;
  for (const Path::Element& element : path.elements()) {
    if (const auto* moved = std::get_if<Path::Move>(&element)) {
      at = moved->to;
      start = moved->to;
      begins = true;
      continue;
    }
    Piece piece{.start = at,
                .element = element,
                .length = 0.0,
                .lengths = {},
                .begins = begins};
    if (std::holds_alternative<Path::CloseSubpath>(element)) {
      piece.element = Path::Line{.to = start};
    }
    piece.lengths.push_back(0.0);
    composition::Point previous = at;
    for (int sample = 1; sample <= length_samples; ++sample) {
      const composition::Point next =
          point_at(piece, static_cast<double>(sample) / length_samples);
      piece.length += distance(previous, next);
      piece.lengths.push_back(piece.length);
      previous = next;
    }
    at = end_of(piece.element, start);
    begins = std::holds_alternative<Path::CloseSubpath>(element);
    found.push_back(std::move(piece));
  }
  return found;
}

auto PathClipper::parameter(const Piece& piece, double distance) -> double {
  if (piece.length <= 0.0) {
    return 0.0;
  }
  const auto upper = std::ranges::lower_bound(piece.lengths, distance);
  if (upper == piece.lengths.begin()) {
    return 0.0;
  }
  if (upper == piece.lengths.end()) {
    return 1.0;
  }
  const auto index = static_cast<std::size_t>(upper - piece.lengths.begin());
  const double low = piece.lengths[index - 1];
  const double high = piece.lengths[index];
  const double within = high > low ? (distance - low) / (high - low) : 0.0;
  return (static_cast<double>(index - 1) + within) / length_samples;
}

auto PathClipper::section(const Piece& piece, double from, double to)
    -> Path::Element {
  return std::visit(
      [&](const auto& each) -> Path::Element {
        using T = std::remove_cvref_t<decltype(each)>;
        if constexpr (std::same_as<T, Path::QuadCurve>) {
          const composition::Point p0 = piece.start;
          const auto split = [](composition::Point a, composition::Point b,
                                composition::Point c, double t) {
            const composition::Point ab = lerp(a, b, t);
            const composition::Point bc = lerp(b, c, t);
            return std::array<composition::Point, 3>{ab, lerp(ab, bc, t), bc};
          };
          const auto head = split(p0, each.control, each.to, to);
          const double inner = to > 0.0 ? from / to : 0.0;
          const auto tail = split(p0, head[0], head[1], inner);
          return Path::QuadCurve{.to = head[1], .control = tail[2]};
        } else if constexpr (std::same_as<T, Path::Curve>) {
          const auto split = [](composition::Point a, composition::Point b,
                                composition::Point c, composition::Point d,
                                double t) {
            const composition::Point ab = lerp(a, b, t);
            const composition::Point bc = lerp(b, c, t);
            const composition::Point cd = lerp(c, d, t);
            const composition::Point abc = lerp(ab, bc, t);
            const composition::Point bcd = lerp(bc, cd, t);
            return std::array<composition::Point, 6>{
                ab, abc, lerp(abc, bcd, t), bcd, cd, d};
          };
          const auto head =
              split(piece.start, each.control1, each.control2, each.to, to);
          const double inner = to > 0.0 ? from / to : 0.0;
          const auto tail =
              split(piece.start, head[0], head[1], head[2], inner);
          return Path::Curve{
              .to = head[2], .control1 = tail[3], .control2 = tail[4]};
        } else {
          return Path::Line{.to = point_at(piece, to)};
        }
      },
      piece.element);
}

auto PathClipper::trimmed(const Path& path, double from, double to) -> Path {
  from = std::clamp(from, 0.0, 1.0);
  to = std::clamp(to, 0.0, 1.0);
  Path made;
  if (to <= from) {
    return made;
  }
  const std::vector<Piece> found = pieces(path);
  double total = 0.0;
  for (const Piece& piece : found) {
    total += piece.length;
  }
  if (total <= 0.0) {
    return made;
  }
  const double begin = from * total;
  const double end = to * total;
  double offset = 0.0;
  bool joined = false;
  for (const Piece& piece : found) {
    const double low = std::max(begin, offset);
    const double high = std::min(end, offset + piece.length);
    if (piece.begins) {
      joined = false;
    }
    if (high > low ||
        (piece.length == 0.0 && low == high && low > begin && low < end)) {
      const double t0 = parameter(piece, low - offset);
      const double t1 = parameter(piece, high - offset);
      if (!joined || low > offset) {
        made.move(point_at(piece, t0));
      }
      const Path::Element part = section(piece, t0, t1);
      std::visit(
          [&made](const auto& each) {
            using T = std::remove_cvref_t<decltype(each)>;
            if constexpr (std::same_as<T, Path::QuadCurve>) {
              made.add_quad_curve(each.to, each.control);
            } else if constexpr (std::same_as<T, Path::Curve>) {
              made.add_curve(each.to, each.control1, each.control2);
            } else if constexpr (std::same_as<T, Path::Line>) {
              made.add_line(each.to);
            }
          },
          part);
      joined = high >= offset + piece.length;
    } else {
      joined = false;
    }
    offset += piece.length;
  }
  return made;
}

}
