#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>

#include <span>
#include <variant>
#include <vector>

namespace arisa::components {

class MeshGradient : public composition::Element {
 public:
  class Content;

  struct BezierPoint {
    composition::Point position;
    composition::Point leading_control_point;
    composition::Point top_control_point;
    composition::Point trailing_control_point;
    composition::Point bottom_control_point;

    friend constexpr auto operator==(const BezierPoint&, const BezierPoint&)
        -> bool = default;
  };

  class Locations {
   public:
    using Value =
        std::variant<std::vector<composition::Point>, std::vector<BezierPoint>>;

    static auto points(std::vector<composition::Point> points) -> Locations {
      return Locations{Value{std::move(points)}};
    }

    static auto bezier_points(std::vector<BezierPoint> points) -> Locations {
      return Locations{Value{std::move(points)}};
    }

    auto value() const noexcept -> const Value& { return value_; }

    friend auto operator==(const Locations&, const Locations&)
        -> bool = default;

   private:
    explicit Locations(Value value) : value_(std::move(value)) {}

    Value value_;
  };

  class Colors {
   public:
    using Value = std::variant<std::vector<theme::ColourValue>,
                               std::vector<theme::Colour>>;

    static auto colors(std::vector<theme::ColourValue> colors) -> Colors {
      return Colors{Value{std::move(colors)}};
    }

    static auto resolved_colors(std::vector<theme::Colour> colors) -> Colors {
      return Colors{Value{std::move(colors)}};
    }

    auto value() const noexcept -> const Value& { return value_; }

    auto values() const -> std::vector<theme::ColourValue>;

    friend auto operator==(const Colors&, const Colors&) -> bool = default;

   private:
    explicit Colors(Value value) : value_(std::move(value)) {}

    Value value_;
  };

  struct Grid {
    int width{0};
    int height{0};
    std::vector<composition::Point> points;
    std::vector<theme::Colour> colours;
  };

  static constexpr int bezier_steps = 8;

  MeshGradient(int width, int height, std::vector<composition::Point> points,
               std::vector<theme::ColourValue> colors,
               theme::ColourValue background = theme::Colour{0.0F, 0.0F, 0.0F,
                                                             0.0F},
               bool smooths_colors = true,
               graphics::Gradient::ColorSpace color_space =
                   graphics::Gradient::ColorSpace::device);

  MeshGradient(int width, int height, Locations locations, Colors colors,
               theme::ColourValue background = theme::Colour{0.0F, 0.0F, 0.0F,
                                                             0.0F},
               bool smooths_colors = true,
               graphics::Gradient::ColorSpace color_space =
                   graphics::Gradient::ColorSpace::device);

  MeshGradient(int width, int height, std::vector<BezierPoint> bezier_points,
               std::vector<theme::ColourValue> colors,
               theme::ColourValue background = theme::Colour{0.0F, 0.0F, 0.0F,
                                                             0.0F},
               bool smooths_colors = true,
               graphics::Gradient::ColorSpace color_space =
                   graphics::Gradient::ColorSpace::device);

  MeshGradient(int width, int height, std::vector<BezierPoint> bezier_points,
               std::vector<theme::Colour> resolved_colors,
               theme::ColourValue background = theme::Colour{0.0F, 0.0F, 0.0F,
                                                             0.0F},
               bool smooths_colors = true,
               graphics::Gradient::ColorSpace color_space =
                   graphics::Gradient::ColorSpace::device);

  auto grain(float amount) && -> MeshGradient;
};

class MeshGradient::Content final : public composition::ElementContent {
 public:
  Content(int width, int height, std::vector<composition::Point> points,
          std::vector<theme::ColourValue> colors, theme::ColourValue background,
          bool smooths_colors, graphics::Gradient::ColorSpace color_space)
      : width_(width),
        height_(height),
        points_(std::move(points)),
        colors_(std::move(colors)),
        background_(background),
        smooths_(smooths_colors),
        space_(color_space) {}

  auto type() const noexcept -> composition::ElementType override;

  auto framed() const noexcept -> bool override { return true; }

  auto same_as(const composition::ElementContent& other) const -> bool override;

  auto paints_like(const composition::ElementContent& other) const
      -> bool override;

  auto width() const noexcept -> int { return width_; }

  auto height() const noexcept -> int { return height_; }

  auto points() const noexcept -> const std::vector<composition::Point>& {
    return points_;
  }

  auto colors() const noexcept -> const std::vector<theme::ColourValue>& {
    return colors_;
  }

  auto background() const noexcept -> theme::ColourValue { return background_; }

  auto smooths_colors() const noexcept -> bool { return smooths_; }

  auto color_space() const noexcept -> graphics::Gradient::ColorSpace {
    return space_;
  }

  auto grain() const noexcept -> float { return grain_; }

  auto bezier_points() const noexcept -> const std::vector<BezierPoint>& {
    return bezier_;
  }

  void set_bezier_points(std::vector<BezierPoint> points) {
    bezier_ = std::move(points);
  }

  auto expanded(std::span<const theme::Colour> colours) const -> Grid;

  auto well_formed() const noexcept -> bool;

  void set_grain(float amount) noexcept { grain_ = amount; }

  friend auto operator==(const Content& left, const Content& right) -> bool {
    return left.width_ == right.width_ && left.height_ == right.height_ &&
           left.points_ == right.points_ && left.colors_ == right.colors_ &&
           left.background_ == right.background_ &&
           left.smooths_ == right.smooths_ && left.space_ == right.space_ &&
           left.grain_ == right.grain_ && left.bezier_ == right.bezier_;
  }

 private:
  int width_;
  int height_;
  std::vector<composition::Point> points_;
  std::vector<theme::ColourValue> colors_;
  theme::ColourValue background_;
  bool smooths_;
  graphics::Gradient::ColorSpace space_;
  float grain_{0.0F};
  std::vector<MeshGradient::BezierPoint> bezier_;

  static auto patch(const MeshGradient::BezierPoint& top_left,
                    const MeshGradient::BezierPoint& top_right,
                    const MeshGradient::BezierPoint& bottom_left,
                    const MeshGradient::BezierPoint& bottom_right, float u,
                    float v) noexcept -> composition::Point;

  static auto cubic(composition::Point from, composition::Point first,
                    composition::Point second, composition::Point to,
                    float t) noexcept -> composition::Point;
};

}
