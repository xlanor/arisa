#pragma once

#include <arisa/composition/coordinate_space.hpp>
#include <arisa/composition/gesture_combinators.hpp>
#include <arisa/composition/gesture_handlers.hpp>
#include <arisa/composition/gesture_input_kinds.hpp>
#include <arisa/composition/gesture_state.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/size.hpp>

#include <algorithm>
#include <chrono>
#include <functional>
#include <utility>

namespace arisa::composition {

class DragGesture : public GestureCombinators<DragGesture> {
 public:
  struct Value {
    std::chrono::nanoseconds time{0};
    Point location{};
    Point start_location{};
    Size translation{};
    Size velocity{};
    Point predicted_end_location{};
    Size predicted_end_translation{};

    friend constexpr auto operator==(const Value&, const Value&)
        -> bool = default;
  };

  static constexpr float default_distance = 10.0F;

  explicit DragGesture(
      float minimum_distance = default_distance,
      CoordinateSpace coordinate_space = CoordinateSpace::local,
      GestureInputKinds input_kinds = GestureInputKinds::all) noexcept
      : minimum_distance_(std::max(minimum_distance, 0.0F)),
        coordinate_space_(std::move(coordinate_space)),
        input_kinds_(input_kinds) {}

  auto on_changed(
      std::move_only_function<void(const Value&)> action) && -> DragGesture {
    handlers_.on_changed(std::move(action));
    return std::move(*this);
  }

  auto on_ended(
      std::move_only_function<void(const Value&)> action) && -> DragGesture {
    handlers_.on_ended(std::move(action));
    return std::move(*this);
  }

  template <class T, class Body>
  auto updating(GestureStateHandle<T> state, Body body) && -> DragGesture {
    handlers_.updating(std::move(state), std::move(body));
    return std::move(*this);
  }

  auto minimum_distance() const noexcept -> float { return minimum_distance_; }

  auto input_kinds() const noexcept -> GestureInputKinds {
    return input_kinds_;
  }

  auto coordinate_space() const noexcept -> const CoordinateSpace& {
    return coordinate_space_;
  }

  auto handlers() noexcept -> GestureHandlers<Value>& { return handlers_; }

 private:
  float minimum_distance_;
  CoordinateSpace coordinate_space_;
  GestureInputKinds input_kinds_;
  GestureHandlers<Value> handlers_;
};

}
