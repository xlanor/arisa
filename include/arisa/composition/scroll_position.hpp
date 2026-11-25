#pragma once

#include <arisa/composition/axis.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/space.hpp>

#include <optional>
#include <utility>
#include <variant>

namespace arisa::composition {

class ScrollPosition {
 public:
  struct View {
    ElementKey id;
    std::optional<motion::UnitPoint> anchor;

    friend auto operator==(const View&, const View&) -> bool = default;
  };

  struct Along {
    Axis axis{Axis::vertical};
    float offset{0.0F};

    friend auto operator==(const Along&, const Along&) -> bool = default;
  };

  ScrollPosition() noexcept = default;

  explicit ScrollPosition(theme::Edge edge) noexcept : target_(edge) {}

  explicit ScrollPosition(Point point) noexcept : target_(point) {}

  ScrollPosition(Axis axis, float offset) noexcept
      : target_(Along{.axis = axis, .offset = offset}) {}

  explicit ScrollPosition(ElementKey id,
                          std::optional<motion::UnitPoint> anchor = {})
      : target_(View{.id = std::move(id), .anchor = anchor}) {}

  void scroll_to(ElementKey id, std::optional<motion::UnitPoint> anchor = {}) {
    target_ = View{.id = std::move(id), .anchor = anchor};
    by_user_ = false;
  }

  void scroll_to(theme::Edge edge) noexcept {
    target_ = edge;
    by_user_ = false;
  }

  void scroll_to(Point point) noexcept {
    target_ = point;
    by_user_ = false;
  }

  void scroll_to_x(float x) noexcept {
    scroll_to(Point{.x = x, .y = point().value_or(scrolled_).y});
  }

  void scroll_to_y(float y) noexcept {
    scroll_to(Point{.x = point().value_or(scrolled_).x, .y = y});
  }

  auto edge() const noexcept -> std::optional<theme::Edge> {
    if (const auto* found = std::get_if<theme::Edge>(&target_)) {
      return *found;
    }
    return std::nullopt;
  }

  auto point() const noexcept -> std::optional<Point> {
    if (const auto* found = std::get_if<Point>(&target_)) {
      return *found;
    }
    return std::nullopt;
  }

  auto x() const noexcept -> std::optional<float> {
    return along(Axis::horizontal);
  }

  auto y() const noexcept -> std::optional<float> {
    return along(Axis::vertical);
  }

  auto view_id() const -> std::optional<ElementKey> {
    if (const auto* found = std::get_if<View>(&target_)) {
      return found->id;
    }
    return std::nullopt;
  }

  template <class T>
  auto view_id_as() const -> std::optional<T> {
    if (const auto* found = std::get_if<View>(&target_)) {
      return found->id.template as<T>();
    }
    return std::nullopt;
  }

  auto anchor() const noexcept -> std::optional<motion::UnitPoint> {
    if (const auto* found = std::get_if<View>(&target_)) {
      return found->anchor;
    }
    return std::nullopt;
  }

  auto is_positioned_by_user() const noexcept -> bool { return by_user_; }

  auto is_automatic() const noexcept -> bool {
    return std::holds_alternative<std::monostate>(target_);
  }

  static auto by_user(Point point, std::optional<ElementKey> view)
      -> ScrollPosition {
    ScrollPosition position;
    if (view.has_value()) {
      position.target_ = View{.id = std::move(*view), .anchor = std::nullopt};
    } else {
      position.target_ = point;
    }
    position.by_user_ = true;
    position.scrolled_ = point;
    return position;
  }

  friend auto operator==(const ScrollPosition&, const ScrollPosition&)
      -> bool = default;

 private:
  auto along(Axis axis) const noexcept -> std::optional<float> {
    if (const auto* found = std::get_if<Point>(&target_)) {
      return axis == Axis::horizontal ? found->x : found->y;
    }
    if (const auto* found = std::get_if<Along>(&target_);
        found != nullptr && found->axis == axis) {
      return found->offset;
    }
    return std::nullopt;
  }

  std::variant<std::monostate, View, theme::Edge, Point, Along> target_;
  Point scrolled_{};
  bool by_user_{false};
};

}
