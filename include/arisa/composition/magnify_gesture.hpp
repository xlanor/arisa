#pragma once

#include <arisa/composition/gesture_combinators.hpp>
#include <arisa/composition/gesture_handlers.hpp>
#include <arisa/composition/gesture_input_kinds.hpp>
#include <arisa/composition/gesture_state.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/motion/unit_point.hpp>

#include <algorithm>
#include <chrono>
#include <functional>
#include <utility>

namespace arisa::composition {

class MagnifyGesture : public GestureCombinators<MagnifyGesture> {
 public:
  struct Value {
    std::chrono::nanoseconds time{0};
    double magnification{1.0};
    double velocity{0.0};
    motion::UnitPoint start_anchor{};
    Point start_location{};

    friend constexpr auto operator==(const Value&, const Value&)
        -> bool = default;
  };

  static constexpr double default_minimum_scale_delta = 0.01;

  explicit MagnifyGesture(
      double minimum_scale_delta = default_minimum_scale_delta,
      GestureInputKinds input_kinds = GestureInputKinds::all) noexcept
      : minimum_scale_delta_(std::max(minimum_scale_delta, 0.0)),
        input_kinds_(input_kinds) {}

  auto on_changed(
      std::move_only_function<void(const Value&)> action) && -> MagnifyGesture {
    handlers_.on_changed(std::move(action));
    return std::move(*this);
  }

  auto on_ended(
      std::move_only_function<void(const Value&)> action) && -> MagnifyGesture {
    handlers_.on_ended(std::move(action));
    return std::move(*this);
  }

  template <class T, class Body>
  auto updating(GestureStateHandle<T> state, Body body) && -> MagnifyGesture {
    handlers_.updating(std::move(state), std::move(body));
    return std::move(*this);
  }

  auto minimum_scale_delta() const noexcept -> double {
    return minimum_scale_delta_;
  }

  auto input_kinds() const noexcept -> GestureInputKinds {
    return input_kinds_;
  }

  auto handlers() noexcept -> GestureHandlers<Value>& { return handlers_; }

 private:
  double minimum_scale_delta_;
  GestureInputKinds input_kinds_;
  GestureHandlers<Value> handlers_;
};

}
