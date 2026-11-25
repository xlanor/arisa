#pragma once

#include <arisa/composition/gesture_combinators.hpp>
#include <arisa/composition/gesture_handlers.hpp>
#include <arisa/composition/gesture_input_kinds.hpp>
#include <arisa/composition/gesture_state.hpp>

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace arisa::composition {

class LongPressGesture : public GestureCombinators<LongPressGesture> {
 public:
  using Value = bool;
  using Pressing = std::move_only_function<void(bool)>;

  static constexpr double default_duration = 0.5;
  static constexpr float default_distance = 10.0F;

  explicit LongPressGesture(
      double minimum_duration = default_duration,
      float maximum_distance = default_distance,
      GestureInputKinds input_kinds = GestureInputKinds::all) noexcept
      : minimum_duration_(std::max(minimum_duration, 0.0)),
        maximum_distance_(std::max(maximum_distance, 0.0F)),
        input_kinds_(input_kinds) {}

  auto on_changed(std::move_only_function<void(const Value&)>
                      action) && -> LongPressGesture {
    handlers_.on_changed(std::move(action));
    return std::move(*this);
  }

  auto on_ended(std::move_only_function<void(const Value&)>
                    action) && -> LongPressGesture {
    handlers_.on_ended(std::move(action));
    return std::move(*this);
  }

  template <class T, class Body>
  auto updating(GestureStateHandle<T> state, Body body) && -> LongPressGesture {
    handlers_.updating(std::move(state), std::move(body));
    return std::move(*this);
  }

  auto on_pressing_changed(Pressing action) && -> LongPressGesture {
    pressing_.push_back(std::move(action));
    return std::move(*this);
  }

  auto minimum_duration() const noexcept -> double { return minimum_duration_; }

  auto maximum_distance() const noexcept -> float { return maximum_distance_; }

  auto input_kinds() const noexcept -> GestureInputKinds {
    return input_kinds_;
  }

  auto handlers() noexcept -> GestureHandlers<Value>& { return handlers_; }

  void pressing(bool pressed) {
    for (Pressing& action : pressing_) {
      action(pressed);
    }
  }

 private:
  double minimum_duration_;
  float maximum_distance_;
  GestureInputKinds input_kinds_;
  GestureHandlers<Value> handlers_;
  std::vector<Pressing> pressing_;
};

}
