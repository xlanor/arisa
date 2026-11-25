#include <arisa/motion/motion_path.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

namespace arisa::motion {

MotionPath::MotionPath(std::vector<Point> points, double tension) noexcept
    : points_(std::move(points)), tension_(tension) {}

auto MotionPath::catmull_rom(std::vector<Point> points, double tension)
    -> MotionPath {
  return MotionPath{std::move(points), std::clamp(tension, -1.0, 1.0)};
}

auto MotionPath::control(std::ptrdiff_t index) const noexcept -> const Point& {
  const auto last = static_cast<std::ptrdiff_t>(points_.size()) - 1;
  return points_[static_cast<std::size_t>(
      std::clamp<std::ptrdiff_t>(index, 0, last))];
}

auto MotionPath::point(double progress) const noexcept -> Point {
  if (points_.empty()) {
    return Point{};
  }
  if (points_.size() == 1) {
    return points_.front();
  }
  const auto segments = static_cast<double>(points_.size() - 1);
  const double scaled = std::clamp(progress, 0.0, 1.0) * segments;
  const double whole = std::min(std::floor(scaled), segments - 1.0);
  const double time = scaled - whole;
  const auto index = static_cast<std::ptrdiff_t>(whole);
  const double s = (1.0 - tension_) * 0.5;
  const double squared = time * time;
  const double cubed = squared * time;
  const double before = (-s * cubed) + (2.0 * s * squared) - (s * time);
  const double start = ((2.0 - s) * cubed) + ((s - 3.0) * squared) + 1.0;
  const double end =
      ((s - 2.0) * cubed) + ((3.0 - (2.0 * s)) * squared) + (s * time);
  const double after = (s * cubed) - (s * squared);
  const Point& p0 = control(index - 1);
  const Point& p1 = control(index);
  const Point& p2 = control(index + 1);
  const Point& p3 = control(index + 2);
  return Point{.x = static_cast<float>((before * p0.x) + (start * p1.x) +
                                       (end * p2.x) + (after * p3.x)),
               .y = static_cast<float>((before * p0.y) + (start * p1.y) +
                                       (end * p2.y) + (after * p3.y))};
}

}
