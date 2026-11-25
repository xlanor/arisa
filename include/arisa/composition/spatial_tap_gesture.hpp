#pragma once

#include <arisa/composition/coordinate_space.hpp>
#include <arisa/composition/gesture_combinators.hpp>
#include <arisa/composition/gesture_handlers.hpp>
#include <arisa/composition/gesture_input_kinds.hpp>
#include <arisa/composition/gesture_state.hpp>
#include <arisa/composition/point.hpp>

#include <algorithm>
#include <functional>
#include <utility>

namespace arisa::composition {

class SpatialTapGesture : public GestureCombinators<SpatialTapGesture> {
 public:
  struct Value {
    Point location{};

    friend constexpr auto operator==(const Value&, const Value&)
        -> bool = default;
  };

  explicit SpatialTapGesture(
      int count = 1, CoordinateSpace coordinate_space = CoordinateSpace::local,
      GestureInputKinds input_kinds = GestureInputKinds::all) noexcept
      : count_(std::max(count, 1)),
        coordinate_space_(std::move(coordinate_space)),
        input_kinds_(input_kinds) {}

  explicit SpatialTapGesture(CoordinateSpace coordinate_space) noexcept
      : SpatialTapGesture(1, std::move(coordinate_space)) {}

  auto on_ended(std::move_only_function<void(const Value&)>
                    action) && -> SpatialTapGesture {
    handlers_.on_ended(std::move(action));
    return std::move(*this);
  }

  template <class T, class Body>
  auto updating(GestureStateHandle<T> state,
                Body body) && -> SpatialTapGesture {
    handlers_.updating(std::move(state), std::move(body));
    return std::move(*this);
  }

  auto count() const noexcept -> int { return count_; }

  auto input_kinds() const noexcept -> GestureInputKinds {
    return input_kinds_;
  }

  auto coordinate_space() const noexcept -> const CoordinateSpace& {
    return coordinate_space_;
  }

  auto handlers() noexcept -> GestureHandlers<Value>& { return handlers_; }

 private:
  int count_;
  CoordinateSpace coordinate_space_;
  GestureInputKinds input_kinds_;
  GestureHandlers<Value> handlers_;
};

}
