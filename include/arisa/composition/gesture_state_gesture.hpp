#pragma once

#include <arisa/composition/gesture.hpp>
#include <arisa/composition/gesture_combinators.hpp>
#include <arisa/composition/gesture_handlers.hpp>
#include <arisa/composition/gesture_layout.hpp>
#include <arisa/composition/gesture_protocol.hpp>
#include <arisa/composition/gesture_state.hpp>
#include <arisa/motion/transaction.hpp>

#include <concepts>
#include <functional>
#include <memory>
#include <utility>

namespace arisa::composition {

template <class Base, class State>
class GestureStateGesture
    : public GestureCombinators<GestureStateGesture<Base, State>> {
 public:
  using Value = GestureValue<Base>;
  using Observer = GestureLayout::Observer<Value>;
  using Body =
      std::move_only_function<void(const Value&, State&, motion::Transaction&)>;

  template <class Update>
    requires std::invocable<Update&, const Value&, State&,
                            motion::Transaction&> ||
                 std::invocable<Update&, const Value&, State&>
  GestureStateGesture(Base base, GestureStateHandle<State> state, Update body)
      : base_(std::move(base)),
        state_(std::move(state)),
        body_(adapt(std::move(body))) {}

  auto base() const noexcept -> const Base& { return base_; }

  auto state() const noexcept -> const GestureStateHandle<State>& {
    return state_;
  }

  auto on_changed(std::move_only_function<void(const Value&)>
                      action) && -> GestureStateGesture {
    handlers_.on_changed(std::move(action));
    return std::move(*this);
  }

  auto on_ended(std::move_only_function<void(const Value&)>
                    action) && -> GestureStateGesture {
    handlers_.on_ended(std::move(action));
    return std::move(*this);
  }

  void lower(GestureLayout& into, Observer observer) && {
    handlers_.updating(state_, std::move(body_));
    auto relay = std::make_shared<GestureRelay<Value>>(std::move(handlers_),
                                                       std::move(observer));
    GestureLayout::lower(std::move(base_), into,
                         GestureRelay<Value>::observer(std::move(relay)));
  }

 private:
  template <class Update>
  static auto adapt(Update body) -> Body {
    if constexpr (std::invocable<Update&, const Value&, State&,
                                 motion::Transaction&>) {
      return Body{std::move(body)};
    } else {
      return Body{[body = std::move(body)](const Value& value, State& state,
                                           motion::Transaction&) mutable {
        body(value, state);
      }};
    }
  }

  Base base_;
  GestureStateHandle<State> state_;
  Body body_;
  GestureHandlers<Value> handlers_;
};

template <class Base, class State, class Update>
GestureStateGesture(Base, GestureStateHandle<State>, Update)
    -> GestureStateGesture<Base, State>;

}
