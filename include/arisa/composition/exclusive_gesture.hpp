#pragma once

#include <arisa/composition/gesture.hpp>
#include <arisa/composition/gesture_combinators.hpp>
#include <arisa/composition/gesture_handlers.hpp>
#include <arisa/composition/gesture_layout.hpp>
#include <arisa/composition/gesture_protocol.hpp>
#include <arisa/composition/gesture_state.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <variant>

namespace arisa::composition {

template <class First, class Second>
class ExclusiveGesture
    : public GestureCombinators<ExclusiveGesture<First, Second>> {
 public:
  class Value {
   public:
    static auto first(GestureValue<First> value) -> Value {
      return Value{std::in_place_index<0>, std::move(value)};
    }

    static auto second(GestureValue<Second> value) -> Value {
      return Value{std::in_place_index<1>, std::move(value)};
    }

    Value() : held_(std::in_place_index<0>) {}

    auto is_first() const noexcept -> bool { return held_.index() == 0; }

    auto first_value() const -> const GestureValue<First>* {
      return std::get_if<0>(&held_);
    }

    auto second_value() const -> const GestureValue<Second>* {
      return std::get_if<1>(&held_);
    }

    friend auto operator==(const Value&, const Value&) -> bool = default;

   private:
    template <std::size_t Index, class Held>
    Value(std::in_place_index_t<Index> index, Held&& held)
        : held_(index, std::forward<Held>(held)) {}

    std::variant<GestureValue<First>, GestureValue<Second>> held_;
  };

  using Observer = GestureLayout::Observer<Value>;

  ExclusiveGesture(First first, Second second)
      : first_(std::move(first)), second_(std::move(second)) {}

  auto on_changed(std::move_only_function<void(const Value&)>
                      action) && -> ExclusiveGesture {
    handlers_.on_changed(std::move(action));
    return std::move(*this);
  }

  auto on_ended(std::move_only_function<void(const Value&)>
                    action) && -> ExclusiveGesture {
    handlers_.on_ended(std::move(action));
    return std::move(*this);
  }

  template <class T, class Body>
  auto updating(GestureStateHandle<T> state, Body body) && -> ExclusiveGesture {
    handlers_.updating(std::move(state), std::move(body));
    return std::move(*this);
  }

  auto first() const noexcept -> const First& { return first_; }

  auto second() const noexcept -> const Second& { return second_; }

  void lower(GestureLayout& into, Observer observer) && {
    auto relay = std::make_shared<GestureRelay<Value>>(std::move(handlers_),
                                                       std::move(observer));
    const std::size_t begin = into.size();
    GestureLayout::lower(std::move(first_), into,
                         GestureLayout::Observer<GestureValue<First>>{
                             .changed =
                                 [relay](const GestureValue<First>& value) {
                                   relay->changed(Value::first(value));
                                 },
                             .ended =
                                 [relay](const GestureValue<First>& value) {
                                   relay->ended(Value::first(value));
                                 },
                             .cancelled = [relay] { relay->cancelled(); }});
    const std::size_t middle = into.size();
    GestureLayout::lower(std::move(second_), into,
                         GestureLayout::Observer<GestureValue<Second>>{
                             .changed =
                                 [relay](const GestureValue<Second>& value) {
                                   relay->changed(Value::second(value));
                                 },
                             .ended =
                                 [relay](const GestureValue<Second>& value) {
                                   relay->ended(Value::second(value));
                                 },
                             .cancelled = [relay] { relay->cancelled(); }});
    into.branch(GestureLayout::Relation::exclusive, begin, middle);
  }

 private:
  First first_;
  Second second_;
  GestureHandlers<Value> handlers_;
};

}
