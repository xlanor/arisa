#pragma once

#include <cstddef>
#include <vector>

namespace arisa::motion {

class MotionPath {
 public:
  struct Point {
    float x{0.0F};
    float y{0.0F};

    friend constexpr auto operator==(const Point&, const Point&)
        -> bool = default;
  };

  static auto catmull_rom(std::vector<Point> points, double tension = 0.0)
      -> MotionPath;

  auto point(double progress) const noexcept -> Point;

  auto points() const noexcept -> const std::vector<Point>& { return points_; }

  auto tension() const noexcept -> double { return tension_; }

  friend auto operator==(const MotionPath&, const MotionPath&)
      -> bool = default;

 private:
  MotionPath(std::vector<Point> points, double tension) noexcept;

  auto control(std::ptrdiff_t index) const noexcept -> const Point&;

  std::vector<Point> points_;
  double tension_;
};

}
