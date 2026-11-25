#pragma once

#include <arisa/composition/point.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/stroke_style.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace arisa::graphics {

class PathClipper {
 public:
  enum class Operation : std::uint8_t { unite, intersect, subtract, exclude };

  using Polygon = std::vector<composition::Point>;

  struct Polyline {
    std::vector<composition::Point> points;
    bool closed{false};
  };

  static constexpr float flatness = 0.5F;
  static constexpr int arc_steps = 32;
  static constexpr int length_samples = 32;

  static auto polylines(const Path& path) -> std::vector<Polyline>;

  static auto combine(const Path& first, const Path& second, bool eo_fill,
                      Operation operation) -> Path;

  static auto normalized(const Path& path, bool eo_fill) -> Path;

  static auto outline(const Path& path, const StrokeStyle& style) -> Path;

  static auto clipped_lines(const Path& lines, const Path& region, bool eo_fill,
                            bool inside) -> Path;

  static auto trimmed(const Path& path, double from, double to) -> Path;

 private:
  enum class Rule : std::uint8_t { odd, nonzero, positive, negative, two };

  struct Piece {
    composition::Point start;
    Path::Element element;
    double length{0.0};
    std::vector<double> lengths;
    bool begins{false};
  };

  static auto boundary(std::span<const Polygon> polygons, Rule rule)
      -> std::vector<Polygon>;

  static auto regions(const Path& path, bool eo_fill) -> std::vector<Polygon>;

  static auto path_of(std::span<const Polygon> polygons) -> Path;

  static auto signed_area(const Polygon& polygon) -> double;

  static auto orientation(std::span<const Polygon> polygons) -> double;

  static auto reversed(std::span<const Polygon> polygons)
      -> std::vector<Polygon>;

  static void flatten_quad(composition::Point from, composition::Point control,
                           composition::Point to,
                           std::vector<composition::Point>& into);

  static void flatten_cubic(composition::Point from, composition::Point first,
                            composition::Point second, composition::Point to,
                            std::vector<composition::Point>& into);

  static auto dashed(std::span<const Polyline> lines,
                     std::span<const float> pattern, float phase)
      -> std::vector<Polyline>;

  static void stroke_line(const Polyline& line, const StrokeStyle& style,
                          std::vector<Polygon>& into);

  static void join(composition::Point at, composition::Point before,
                   composition::Point after, const StrokeStyle& style,
                   std::vector<Polygon>& into);

  static void cap(composition::Point at, composition::Point toward,
                  const StrokeStyle& style, std::vector<Polygon>& into);

  static auto disc(composition::Point centre, float radius) -> Polygon;

  static auto pieces(const Path& path) -> std::vector<Piece>;

  static auto point_at(const Piece& piece, double t) -> composition::Point;

  static auto parameter(const Piece& piece, double distance) -> double;

  static auto section(const Piece& piece, double from, double to)
      -> Path::Element;

  static auto end_of(const Path::Element& element, composition::Point start)
      -> composition::Point;

  static auto lerp(composition::Point a, composition::Point b, double t)
      -> composition::Point;

  static auto distance(composition::Point a, composition::Point b) -> double;
};

}
