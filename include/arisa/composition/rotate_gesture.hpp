#pragma once

#include <arisa/composition/gesture_combinators.hpp>
#include <arisa/composition/gesture_handlers.hpp>
#include <arisa/composition/gesture_input_kinds.hpp>
#include <arisa/composition/gesture_state.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/unit_point.hpp>

#include <chrono>
#include <cmath>
#include <functional>
#include <utility>

namespace arisa::composition {

class RotateGesture : public GestureCombinators<RotateGesture> {
 public:
  struct Value {
    std::chrono::nanoseconds time{0};
    motion::Angle rotation{};
    motion::Angle velocity{};
    motion::UnitPoint start_anchor{};
    Point start_location{};

    friend constexpr auto operator==(const Value&, const Value&)
        -> bool = default;
  };

  static constexpr motion::Angle default_minimum_angle_delta =
      motion::Angle::degrees(1.0);

  explicit RotateGesture(
      motion::Angle minimum_angle_delta = default_minimum_angle_delta,
      GestureInputKinds input_kinds = GestureInputKinds::all) noexcept
      : minimum_angle_delta_(
            motion::Angle::radians(std::abs(minimum_angle_delta.radians()))),
        input_kinds_(input_kinds) {}

  auto on_changed(
      std::move_only_function<void(const Value&)> action) && -> RotateGesture {
    handlers_.on_changed(std::move(action));
    return std::move(*this);
  }

  auto on_ended(
      std::move_only_function<void(const Value&)> action) && -> RotateGesture {
    handlers_.on_ended(std::move(action));
    return std::move(*this);
  }

  template <class T, class Body>
  auto updating(GestureStateHandle<T> state, Body body) && -> RotateGesture {
    handlers_.updating(std::move(state), std::move(body));
    return std::move(*this);
  }

  auto minimum_angle_delta() const noexcept -> motion::Angle {
    return minimum_angle_delta_;
  }

  auto input_kinds() const noexcept -> GestureInputKinds {
    return input_kinds_;
  }

  auto handlers() noexcept -> GestureHandlers<Value>& { return handlers_; }

 private:
  motion::Angle minimum_angle_delta_;
  GestureInputKinds input_kinds_;
  GestureHandlers<Value> handlers_;
};

}
