#include <bridge/nanovg/path_tessellator.hpp>

#include <nanovg_path.h>
#include <tesselator.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <variant>

namespace arisa::bridge::nanovg {

void PathTessellator::ContextTraits::release(handle_type& handle) noexcept {
  nvgpDelete(handle);
}

auto PathTessellator::create() -> std::optional<PathTessellator> {
  NVGpathContext* context = nvgpCreate(tolerance, distance, fringe);
  if (context == nullptr) {
    return std::nullopt;
  }
  return PathTessellator{context};
}

auto PathTessellator::flatten(const graphics::Path& path, float scale,
                              float origin_x, float origin_y)
    -> std::vector<Contour> {
  NVGpathContext* context = context_.get();
  nvgpBeginPath(context);
  const auto x = [&](const composition::Point& point) {
    return origin_x + (point.x * scale);
  };
  const auto y = [&](const composition::Point& point) {
    return origin_y + (point.y * scale);
  };
  composition::Point last{};
  for (const graphics::Path::Element& element : path.elements()) {
    std::visit(
        [&](const auto& each) {
          using T = std::remove_cvref_t<decltype(each)>;
          if constexpr (std::same_as<T, graphics::Path::Move>) {
            nvgpMoveTo(context, x(each.to), y(each.to));
            last = each.to;
          } else if constexpr (std::same_as<T, graphics::Path::Line>) {
            nvgpLineTo(context, x(each.to), y(each.to));
            last = each.to;
          } else if constexpr (std::same_as<T, graphics::Path::QuadCurve>) {
            constexpr float two_thirds = 2.0F / 3.0F;
            const composition::Point first{
                .x = last.x + ((each.control.x - last.x) * two_thirds),
                .y = last.y + ((each.control.y - last.y) * two_thirds)};
            const composition::Point second{
                .x = each.to.x + ((each.control.x - each.to.x) * two_thirds),
                .y = each.to.y + ((each.control.y - each.to.y) * two_thirds)};
            nvgpBezierTo(context, x(first), y(first), x(second), y(second),
                         x(each.to), y(each.to));
            last = each.to;
          } else if constexpr (std::same_as<T, graphics::Path::Curve>) {
            nvgpBezierTo(context, x(each.control1), y(each.control1),
                         x(each.control2), y(each.control2), x(each.to),
                         y(each.to));
            last = each.to;
          } else {
            nvgpClosePath(context);
          }
        },
        element);
  }
  nvgpFlatten(context, 0);
  std::vector<Contour> contours;
  const int count = nvgpPathCount(context);
  contours.reserve(static_cast<std::size_t>(count));
  for (int index = 0; index < count; ++index) {
    const NVGpath* flattened = nvgpPath(context, index);
    Contour contour{.points = {}, .closed = flattened->closed != 0};
    contour.points.reserve(static_cast<std::size_t>(flattened->count));
    for (int point = 0; point < flattened->count; ++point) {
      std::array<float, 2> at{};
      nvgpPoint(context, flattened->first + point, &at[0], &at[1]);
      contour.points.push_back(at);
    }
    if (!contour.points.empty()) {
      contours.push_back(std::move(contour));
    }
  }
  return contours;
}

void PathTessellator::load(std::span<const Contour> contours, bool enforce) {
  NVGpathContext* context = context_.get();
  nvgpBeginPath(context);
  for (const Contour& contour : contours) {
    if (contour.points.empty()) {
      continue;
    }
    nvgpMoveTo(context, contour.points.front()[0], contour.points.front()[1]);
    for (const auto& point : contour.points | std::views::drop(1)) {
      nvgpLineTo(context, point[0], point[1]);
    }
    if (contour.closed) {
      nvgpClosePath(context);
    }
  }
  nvgpFlatten(context, enforce ? 1 : 0);
}

void PathTessellator::strip(const NVGvertex* first, int count, Mesh& mesh) {
  if (count < 3) {
    return;
  }
  const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
  for (const NVGvertex& each :
       std::span{first, static_cast<std::size_t>(count)}) {
    mesh.vertices.push_back(
        Vertex{.x = each.x, .y = each.y, .u = each.u, .v = each.v});
  }
  for (int index = 0; index + 2 < count; ++index) {
    const auto at = base + static_cast<std::uint32_t>(index);
    mesh.indices.insert(mesh.indices.end(), {at, at + 1, at + 2});
  }
}

void PathTessellator::fan(const NVGvertex* first, int count, Mesh& mesh) {
  if (count < 3) {
    return;
  }
  const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
  for (const NVGvertex& each :
       std::span{first, static_cast<std::size_t>(count)}) {
    mesh.vertices.push_back(
        Vertex{.x = each.x, .y = each.y, .u = each.u, .v = each.v});
  }
  for (int index = 1; index + 1 < count; ++index) {
    const auto at = base + static_cast<std::uint32_t>(index);
    mesh.indices.insert(mesh.indices.end(), {base, at, at + 1});
  }
}

auto PathTessellator::stroke(std::span<const Contour> contours,
                             const graphics::StrokeStyle& style, float scale,
                             bool antialiased) -> Mesh {
  Mesh mesh;
  float width = std::clamp(style.line_width * scale, 0.0F, widest);
  if (width <= 0.0F) {
    return mesh;
  }
  if (!antialiased) {
    width = std::max(std::round(width), 1.0F);
    mesh.multiplier = aliased_multiplier;
    width = std::max(width - fringe, 0.0F);
  } else if (width < fringe) {
    const float share = std::clamp(width / fringe, 0.0F, 1.0F);
    mesh.alpha = share * share;
    width = fringe;
  }
  if (antialiased) {
    mesh.multiplier = ((width * 0.5F) + (fringe * 0.5F)) / fringe;
  }
  load(contours, false);
  const int cap = style.line_cap == graphics::LineCap::round    ? NVG_ROUND
                  : style.line_cap == graphics::LineCap::square ? NVG_SQUARE
                                                                : NVG_BUTT;
  const int join = style.line_join == graphics::LineJoin::round   ? NVG_ROUND
                   : style.line_join == graphics::LineJoin::bevel ? NVG_BEVEL
                                                                  : NVG_MITER;
  NVGpathContext* context = context_.get();
  if (nvgpExpandStroke(context, width, cap, join,
                       std::max(style.miter_limit, 1.0F)) == 0) {
    return mesh;
  }
  std::vector<const Contour*> loaded;
  for (const Contour& contour : contours) {
    if (!contour.points.empty()) {
      loaded.push_back(&contour);
    }
  }
  const int count = nvgpPathCount(context);
  const bool matched = std::cmp_equal(count, loaded.size());
  double offset = 0.0;
  for (int index = 0; index < count; ++index) {
    const NVGpath* path = nvgpPath(context, index);
    const std::size_t begin = mesh.vertices.size();
    if (path->count >= 2 || (path->count == 1 && cap != NVG_BUTT)) {
      strip(path->stroke, path->nstroke, mesh);
    }
    if (matched) {
      const Contour& contour = *loaded[static_cast<std::size_t>(index)];
      measure(contour, offset, std::span{mesh.vertices}.subspan(begin));
      offset += contour_length(contour);
    }
  }
  mesh.length = static_cast<float>(offset);
  mesh.width = width + fringe;
  return mesh;
}

void PathTessellator::split(Mesh& mesh, float edge) {
  const float limit = edge * edge;
  std::vector<std::uint32_t> pending = std::move(mesh.indices);
  std::vector<std::uint32_t> done;
  done.reserve(pending.size());
  const auto gap = [&mesh](std::uint32_t a, std::uint32_t b) {
    const Vertex& p = mesh.vertices[a];
    const Vertex& q = mesh.vertices[b];
    return ((p.x - q.x) * (p.x - q.x)) + ((p.y - q.y) * (p.y - q.y));
  };
  const auto middle = [&mesh](std::uint32_t a, std::uint32_t b) {
    const Vertex p = mesh.vertices[a];
    const Vertex q = mesh.vertices[b];
    mesh.vertices.push_back(Vertex{.x = (p.x + q.x) * 0.5F,
                                   .y = (p.y + q.y) * 0.5F,
                                   .u = (p.u + q.u) * 0.5F,
                                   .v = (p.v + q.v) * 0.5F});
    return static_cast<std::uint32_t>(mesh.vertices.size() - 1);
  };
  while (!pending.empty()) {
    const std::uint32_t c = pending.back();
    pending.pop_back();
    const std::uint32_t b = pending.back();
    pending.pop_back();
    const std::uint32_t a = pending.back();
    pending.pop_back();
    const float ab = gap(a, b);
    const float bc = gap(b, c);
    const float ca = gap(c, a);
    const float longest = std::max({ab, bc, ca});
    if (longest <= limit) {
      done.insert(done.end(), {a, b, c});
      continue;
    }
    if (longest == ab) {
      const std::uint32_t m = middle(a, b);
      pending.insert(pending.end(), {a, m, c, m, b, c});
    } else if (longest == bc) {
      const std::uint32_t m = middle(b, c);
      pending.insert(pending.end(), {a, b, m, a, m, c});
    } else {
      const std::uint32_t m = middle(c, a);
      pending.insert(pending.end(), {a, b, m, m, b, c});
    }
  }
  mesh.indices = std::move(done);
}

void PathTessellator::follow(Mesh& mesh, std::span<const Contour> guide,
                             float edge) {
  split(mesh, edge);
  struct Segment {
    std::array<float, 2> from;
    std::array<float, 2> to;
    double start;
    double span;
  };
  std::vector<Segment> segments;
  double offset = 0.0;
  for (const Contour& contour : guide) {
    for (std::size_t index = 1; index < contour.points.size(); ++index) {
      const auto& from = contour.points[index - 1];
      const auto& to = contour.points[index];
      const double span = std::hypot(to[0] - from[0], to[1] - from[1]);
      segments.push_back(
          Segment{.from = from, .to = to, .start = offset, .span = span});
      offset += span;
    }
  }
  mesh.length = static_cast<float>(offset);
  for (Vertex& vertex : mesh.vertices) {
    double best = std::numeric_limits<double>::max();
    double along = 0.0;
    for (const Segment& segment : segments) {
      const double dx = segment.to[0] - segment.from[0];
      const double dy = segment.to[1] - segment.from[1];
      const double square = (dx * dx) + (dy * dy);
      const double t = square > 0.0
                           ? std::clamp((((vertex.x - segment.from[0]) * dx) +
                                         ((vertex.y - segment.from[1]) * dy)) /
                                            square,
                                        0.0, 1.0)
                           : 0.0;
      const double px = segment.from[0] + (dx * t) - vertex.x;
      const double py = segment.from[1] + (dy * t) - vertex.y;
      const double apart = (px * px) + (py * py);
      if (apart < best) {
        best = apart;
        along = segment.start + (segment.span * t);
      }
    }
    vertex.s = static_cast<float>(along);
  }
}

void PathTessellator::measure(const Contour& contour, double offset,
                              std::span<Vertex> vertices) {
  std::vector<std::array<float, 2>> points = contour.points;
  if (contour.closed && points.size() > 1) {
    points.push_back(points.front());
  }
  if (points.size() < 2) {
    for (Vertex& vertex : vertices) {
      vertex.s = static_cast<float>(offset);
    }
    return;
  }
  std::vector<double> starts(points.size(), offset);
  for (std::size_t index = 1; index < points.size(); ++index) {
    starts[index] =
        starts[index - 1] + std::hypot(points[index][0] - points[index - 1][0],
                                       points[index][1] - points[index - 1][1]);
  }
  constexpr std::size_t window = 12;
  const std::size_t segments = points.size() - 1;
  std::size_t cursor = 0;
  for (Vertex& vertex : vertices) {
    double best = std::numeric_limits<double>::max();
    double along = starts[cursor];
    std::size_t chosen = cursor;
    const std::size_t low = cursor > 2 ? cursor - 2 : 0;
    const std::size_t high = std::min(segments, cursor + window);
    for (std::size_t index = low; index < high; ++index) {
      const double ax = points[index][0];
      const double ay = points[index][1];
      const double dx = points[index + 1][0] - ax;
      const double dy = points[index + 1][1] - ay;
      const double span = (dx * dx) + (dy * dy);
      const double t =
          span > 0.0
              ? std::clamp(
                    (((vertex.x - ax) * dx) + ((vertex.y - ay) * dy)) / span,
                    0.0, 1.0)
              : 0.0;
      const double px = ax + (dx * t) - vertex.x;
      const double py = ay + (dy * t) - vertex.y;
      const double gap = (px * px) + (py * py);
      if (gap < best) {
        best = gap;
        chosen = index;
        along = starts[index] + ((starts[index + 1] - starts[index]) * t);
      }
    }
    cursor = chosen;
    vertex.s = static_cast<float>(along);
  }
}

void PathTessellator::fringes(Mesh& mesh) {
  NVGpathContext* context = context_.get();
  const int count = nvgpPathCount(context);
  for (int index = 0; index < count; ++index) {
    const NVGpath* path = nvgpPath(context, index);
    strip(path->stroke, path->nstroke, mesh);
  }
}

auto PathTessellator::area(const Contour& contour) -> double {
  double total = 0.0;
  const auto& points = contour.points;
  for (std::size_t index = 2; index < points.size(); ++index) {
    const auto& a = points[0];
    const auto& b = points[index - 1];
    const auto& c = points[index];
    total += static_cast<double>(((c[0] - a[0]) * (b[1] - a[1])) -
                                 ((b[0] - a[0]) * (c[1] - a[1])));
  }
  return total * 0.5;
}

auto PathTessellator::tessellate(std::span<const Contour> contours,
                                 bool even_odd, bool boundary)
    -> std::optional<std::vector<Contour>> {
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
  const support::UniqueHandle<TessTraits> tess{tessNewTess(nullptr)};
  if (!tess.valid()) {
    return std::nullopt;
  }
  for (const Contour& contour : contours) {
    if (contour.points.size() < 3) {
      continue;
    }
    tessAddContour(tess.get(), 2, contour.points.data(),
                   static_cast<int>(sizeof(std::array<float, 2>)),
                   static_cast<int>(contour.points.size()));
  }
  const int rule = even_odd ? TESS_WINDING_ODD : TESS_WINDING_NONZERO;
  if (tessTesselate(tess.get(), rule,
                    boundary ? TESS_BOUNDARY_CONTOURS : TESS_POLYGONS, 3, 2,
                    nullptr) == 0) {
    return std::nullopt;
  }
  const TESSreal* vertices = tessGetVertices(tess.get());
  const TESSindex* elements = tessGetElements(tess.get());
  const int count = tessGetElementCount(tess.get());
  std::vector<Contour> found;
  if (boundary) {
    for (int index = 0; index < count; ++index) {
      const TESSindex first = elements[index * 2];
      const TESSindex size = elements[(index * 2) + 1];
      Contour contour{.points = {}, .closed = true};
      for (TESSindex point = 0; point < size; ++point) {
        const auto at = static_cast<std::size_t>(first + point) * 2;
        contour.points.push_back({vertices[at], vertices[at + 1]});
      }
      found.push_back(std::move(contour));
    }
    return found;
  }
  Contour triangles{.points = {}, .closed = true};
  for (int index = 0; index < count * 3; ++index) {
    const TESSindex vertex = elements[index];
    const auto at = static_cast<std::size_t>(vertex) * 2;
    triangles.points.push_back({vertices[at], vertices[at + 1]});
  }
  found.push_back(std::move(triangles));
  return found;
}

auto PathTessellator::fill(std::span<const Contour> contours,
                           graphics::FillStyle style) -> Mesh {
  Mesh mesh;
  NVGpathContext* context = context_.get();
  const bool single = contours.size() == 1 &&
                      contours.front().points.size() > 2 &&
                      simple(contours.front());
  if (single) {
    load(contours, true);
    if (nvgpExpandFill(context, style.antialiased ? 1 : 0, 0) != 0 &&
        nvgpPathCount(context) == 1 && nvgpPath(context, 0)->convex != 0) {
      const NVGpath* path = nvgpPath(context, 0);
      fan(path->fill, path->nfill, mesh);
      if (style.antialiased) {
        fringes(mesh);
      }
      return mesh;
    }
  }
  const auto solid = tessellate(contours, style.eo_fill, false);
  if (!solid.has_value() || solid->empty()) {
    return mesh;
  }
  const auto& points = solid->front().points;
  const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
  for (const auto& point : points) {
    mesh.vertices.push_back(Vertex{.x = point[0], .y = point[1]});
  }
  for (std::uint32_t index = 0; index < points.size(); ++index) {
    mesh.indices.push_back(base + index);
  }
  if (!style.antialiased) {
    return mesh;
  }
  auto outline = tessellate(contours, style.eo_fill, true);
  if (!outline.has_value() || outline->empty()) {
    return mesh;
  }
  const auto largest = std::ranges::max_element(
      *outline, {}, [](const Contour& each) { return std::abs(area(each)); });
  if (area(*largest) < 0.0) {
    for (Contour& each : *outline) {
      std::ranges::reverse(each.points);
    }
  }
  load(*outline, false);
  if (nvgpExpandFill(context, 1, 1) != 0) {
    fringes(mesh);
  }
  return mesh;
}

auto PathTessellator::simple(const Contour& contour) -> bool {
  const auto& points = contour.points;
  const std::size_t count = points.size();
  double turning = 0.0;
  for (std::size_t index = 0; index < count; ++index) {
    const auto& a = points[(index + count - 1) % count];
    const auto& b = points[index];
    const auto& c = points[(index + 1) % count];
    const double ux = b[0] - a[0];
    const double uy = b[1] - a[1];
    const double vx = c[0] - b[0];
    const double vy = c[1] - b[1];
    turning += std::atan2((ux * vy) - (uy * vx), (ux * vx) + (uy * vy));
  }
  return std::abs(std::abs(turning) - (2.0 * std::numbers::pi)) < 0.5;
}

auto PathTessellator::contour_length(const Contour& contour) -> double {
  double total = 0.0;
  const auto& points = contour.points;
  for (std::size_t index = 1; index < points.size(); ++index) {
    total += std::hypot(points[index][0] - points[index - 1][0],
                        points[index][1] - points[index - 1][1]);
  }
  if (contour.closed && points.size() > 1) {
    total += std::hypot(points.front()[0] - points.back()[0],
                        points.front()[1] - points.back()[1]);
  }
  return total;
}

auto PathTessellator::length(std::span<const Contour> contours) -> double {
  double total = 0.0;
  for (const Contour& contour : contours) {
    total += contour_length(contour);
  }
  return total;
}

void PathTessellator::cut(const Contour& contour, double from, double to,
                          std::vector<Contour>& into) {
  std::vector<std::array<float, 2>> points = contour.points;
  if (contour.closed && !points.empty()) {
    points.push_back(points.front());
  }
  Contour piece{.points = {}, .closed = false};
  double walked = 0.0;
  const auto lerp = [](const std::array<float, 2>& a,
                       const std::array<float, 2>& b, double amount) {
    const auto share = static_cast<float>(amount);
    return std::array<float, 2>{a[0] + ((b[0] - a[0]) * share),
                                a[1] + ((b[1] - a[1]) * share)};
  };
  for (std::size_t index = 1; index < points.size(); ++index) {
    const auto& a = points[index - 1];
    const auto& b = points[index];
    const double segment = std::hypot(b[0] - a[0], b[1] - a[1]);
    const double start = walked;
    const double end = walked + segment;
    walked = end;
    if (end < from || start > to || segment <= 0.0) {
      continue;
    }
    if (piece.points.empty()) {
      piece.points.push_back(from > start ? lerp(a, b, (from - start) / segment)
                                          : a);
    }
    piece.points.push_back(to < end ? lerp(a, b, (to - start) / segment) : b);
  }
  if (piece.points.size() > 1) {
    into.push_back(std::move(piece));
  }
}

auto PathTessellator::trim(std::span<const Contour> contours, double from,
                           double to) -> std::vector<Contour> {
  from = std::clamp(from, 0.0, 1.0);
  to = std::clamp(to, 0.0, 1.0);
  if (from <= 0.0 && to >= 1.0) {
    return {contours.begin(), contours.end()};
  }
  std::vector<Contour> kept;
  if (to <= from) {
    return kept;
  }
  const double total = length(contours);
  const double begin = from * total;
  const double finish = to * total;
  double offset = 0.0;
  for (const Contour& contour : contours) {
    const double span = contour_length(contour);
    if (offset + span >= begin && offset <= finish) {
      cut(contour, begin - offset, finish - offset, kept);
    }
    offset += span;
  }
  return kept;
}

auto PathTessellator::dash(std::span<const Contour> contours,
                           std::span<const float> pattern, float phase)
    -> std::vector<Contour> {
  std::vector<float> cycle{pattern.begin(), pattern.end()};
  if (cycle.size() % 2 == 1) {
    cycle.insert(cycle.end(), pattern.begin(), pattern.end());
  }
  double period = 0.0;
  for (const float each : cycle) {
    if (each < 0.0F) {
      return {contours.begin(), contours.end()};
    }
    period += each;
  }
  if (cycle.empty() || period <= 0.0) {
    return {contours.begin(), contours.end()};
  }
  std::vector<Contour> dashes;
  for (const Contour& contour : contours) {
    const double span = contour_length(contour);
    double start = -std::fmod(static_cast<double>(phase), period);
    if (start > 0.0) {
      start -= period;
    }
    std::size_t index = 0;
    while (start < span) {
      const double extent = cycle[index];
      if (index % 2 == 0 && extent > 0.0) {
        const double begin = std::max(start, 0.0);
        const double end = std::min(start + extent, span);
        if (end > begin) {
          cut(contour, begin, end, dashes);
        }
      }
      start += extent;
      index = (index + 1) % cycle.size();
    }
  }
  return dashes;
}

}
