#pragma once

#include <arisa/composition/gesture.hpp>
#include <arisa/composition/gesture_combinators.hpp>
#include <arisa/composition/gesture_handlers.hpp>
#include <arisa/composition/gesture_layout.hpp>
#include <arisa/composition/gesture_protocol.hpp>
#include <arisa/composition/gesture_state.hpp>

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace arisa::composition {

template <class First, class Second>
class SimultaneousGesture
    : public GestureCombinators<SimultaneousGesture<First, Second>> {
 public:
  struct Value {
    std::optional<GestureValue<First>> first;
    std::optional<GestureValue<Second>> second;

    friend auto operator==(const Value&, const Value&) -> bool = default;
  };

  using Observer = GestureLayout::Observer<Value>;

  SimultaneousGesture(First first, Second second)
      : first_(std::move(first)), second_(std::move(second)) {}

  auto on_changed(std::move_only_function<void(const Value&)>
                      action) && -> SimultaneousGesture {
    handlers_.on_changed(std::move(action));
    return std::move(*this);
  }

  auto on_ended(std::move_only_function<void(const Value&)>
                    action) && -> SimultaneousGesture {
    handlers_.on_ended(std::move(action));
    return std::move(*this);
  }

  template <class T, class Body>
  auto updating(GestureStateHandle<T> state,
                Body body) && -> SimultaneousGesture {
    handlers_.updating(std::move(state), std::move(body));
    return std::move(*this);
  }

  auto first() const noexcept -> const First& { return first_; }

  auto second() const noexcept -> const Second& { return second_; }

  void lower(GestureLayout& into, Observer observer) && {
    auto state =
        std::make_shared<State>(std::move(handlers_), std::move(observer));
    const std::size_t begin = into.size();
    GestureLayout::lower(std::move(first_), into,
                         GestureLayout::Observer<GestureValue<First>>{
                             .changed =
                                 [state](const GestureValue<First>& value) {
                                   state->value.first = value;
                                   state->active[0] = true;
                                   state->changed();
                                 },
                             .ended =
                                 [state](const GestureValue<First>& value) {
                                   state->value.first = value;
                                   state->active[0] = false;
                                   state->settle(true);
                                 },
                             .cancelled =
                                 [state] {
                                   state->value.first.reset();
                                   state->active[0] = false;
                                   state->settle(false);
                                 }});
    const std::size_t middle = into.size();
    GestureLayout::lower(std::move(second_), into,
                         GestureLayout::Observer<GestureValue<Second>>{
                             .changed =
                                 [state](const GestureValue<Second>& value) {
                                   state->value.second = value;
                                   state->active[1] = true;
                                   state->changed();
                                 },
                             .ended =
                                 [state](const GestureValue<Second>& value) {
                                   state->value.second = value;
                                   state->active[1] = false;
                                   state->settle(true);
                                 },
                             .cancelled =
                                 [state] {
                                   state->value.second.reset();
                                   state->active[1] = false;
                                   state->settle(false);
                                 }});
    into.branch(GestureLayout::Relation::simultaneous, begin, middle);
  }

 private:
  class State {
   public:
    State(GestureHandlers<Value> handlers, Observer outer) noexcept
        : relay_(std::move(handlers), std::move(outer)) {}

    void changed() { relay_.changed(value); }

    void settle(bool succeeded) {
      if (active[0] || active[1]) {
        return;
      }
      if (succeeded) {
        relay_.ended(value);
      } else if (!value.first.has_value() && !value.second.has_value()) {
        relay_.cancelled();
      } else {
        relay_.ended(value);
      }
      value = Value{};
    }

    Value value;
    std::array<bool, 2> active{false, false};

   private:
    GestureRelay<Value> relay_;
  };

  First first_;
  Second second_;
  GestureHandlers<Value> handlers_;
};

}
