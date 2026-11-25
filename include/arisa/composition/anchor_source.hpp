#pragma once

#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/motion/unit_point.hpp>

#include <utility>
#include <vector>

namespace arisa::composition {

template <class Value>
class AnchorSource;

template <>
class AnchorSource<Rect> {
 public:
  static constexpr auto bounds() noexcept -> AnchorSource {
    return AnchorSource{
        Rect{.x = 0.0F, .y = 0.0F, .width = 1.0F, .height = 1.0F}, Rect{}};
  }

  static constexpr auto rect(Rect local) noexcept -> AnchorSource {
    return AnchorSource{Rect{}, local};
  }

  constexpr auto resolve(const Rect& frame) const noexcept -> Rect {
    return Rect{.x = frame.x + (frame.width * unit_.x) + offset_.x,
                .y = frame.y + (frame.height * unit_.y) + offset_.y,
                .width = (frame.width * unit_.width) + offset_.width,
                .height = (frame.height * unit_.height) + offset_.height};
  }

  friend constexpr auto operator==(const AnchorSource&, const AnchorSource&)
      -> bool = default;

 private:
  constexpr AnchorSource(Rect unit, Rect offset) noexcept
      : unit_(unit), offset_(offset) {}

  Rect unit_;
  Rect offset_;
};

template <>
class AnchorSource<Point> {
 public:
  static constexpr auto unit_point(motion::UnitPoint unit) noexcept
      -> AnchorSource {
    return AnchorSource{unit, Point{}};
  }

  static constexpr auto point(Point local) noexcept -> AnchorSource {
    return AnchorSource{motion::UnitPoint::zero, local};
  }

  static constexpr auto top_leading() noexcept -> AnchorSource {
    return unit_point(motion::UnitPoint::top_leading);
  }

  static constexpr auto top() noexcept -> AnchorSource {
    return unit_point(motion::UnitPoint::top);
  }

  static constexpr auto top_trailing() noexcept -> AnchorSource {
    return unit_point(motion::UnitPoint::top_trailing);
  }

  static constexpr auto leading() noexcept -> AnchorSource {
    return unit_point(motion::UnitPoint::leading);
  }

  static constexpr auto center() noexcept -> AnchorSource {
    return unit_point(motion::UnitPoint::center);
  }

  static constexpr auto trailing() noexcept -> AnchorSource {
    return unit_point(motion::UnitPoint::trailing);
  }

  static constexpr auto bottom_leading() noexcept -> AnchorSource {
    return unit_point(motion::UnitPoint::bottom_leading);
  }

  static constexpr auto bottom() noexcept -> AnchorSource {
    return unit_point(motion::UnitPoint::bottom);
  }

  static constexpr auto bottom_trailing() noexcept -> AnchorSource {
    return unit_point(motion::UnitPoint::bottom_trailing);
  }

  constexpr auto resolve(const Rect& frame) const noexcept -> Point {
    return Point{.x = frame.x + (frame.width * unit_.x) + offset_.x,
                 .y = frame.y + (frame.height * unit_.y) + offset_.y};
  }

  friend constexpr auto operator==(const AnchorSource&, const AnchorSource&)
      -> bool = default;

 private:
  constexpr AnchorSource(motion::UnitPoint unit, Point offset) noexcept
      : unit_(unit), offset_(offset) {}

  motion::UnitPoint unit_;
  Point offset_;
};

template <class Value>
class AnchorSource<std::vector<Value>> {
 public:
  explicit AnchorSource(std::vector<AnchorSource<Value>> sources)
      : sources_(std::move(sources)) {}

  auto sources() const noexcept -> const std::vector<AnchorSource<Value>>& {
    return sources_;
  }

  auto resolve(const Rect& frame) const -> std::vector<Value> {
    std::vector<Value> resolved;
    resolved.reserve(sources_.size());
    for (const AnchorSource<Value>& source : sources_) {
      resolved.push_back(source.resolve(frame));
    }
    return resolved;
  }

  friend auto operator==(const AnchorSource&, const AnchorSource&)
      -> bool = default;

 private:
  std::vector<AnchorSource<Value>> sources_;
};

}
