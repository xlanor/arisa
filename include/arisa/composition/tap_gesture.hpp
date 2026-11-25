#pragma once

#include <arisa/composition/gesture_combinators.hpp>
#include <arisa/composition/gesture_handlers.hpp>
#include <arisa/composition/gesture_input_kinds.hpp>
#include <arisa/composition/gesture_state.hpp>

#include <algorithm>
#include <functional>
#include <utility>
#include <variant>

namespace arisa::composition {

class TapGesture : public GestureCombinators<TapGesture> {
 public:
  using Value = std::monostate;

  explicit TapGesture(int count = 1, GestureInputKinds input_kinds =
                                         GestureInputKinds::all) noexcept
      : count_(std::max(count, 1)), input_kinds_(input_kinds) {}

  auto on_ended(std::move_only_function<void()> action) && -> TapGesture {
    handlers_.on_ended(
        [action = std::move(action)](const Value&) mutable { action(); });
    return std::move(*this);
  }

  template <class T, class Body>
  auto updating(GestureStateHandle<T> state, Body body) && -> TapGesture {
    handlers_.updating(std::move(state), std::move(body));
    return std::move(*this);
  }

  auto count() const noexcept -> int { return count_; }

  auto input_kinds() const noexcept -> GestureInputKinds {
    return input_kinds_;
  }

  auto handlers() noexcept -> GestureHandlers<Value>& { return handlers_; }

 private:
  int count_;
  GestureInputKinds input_kinds_;
  GestureHandlers<Value> handlers_;
};

}
