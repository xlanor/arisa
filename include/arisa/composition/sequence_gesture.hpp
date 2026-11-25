#pragma once

#include <arisa/composition/gesture.hpp>
#include <arisa/composition/gesture_combinators.hpp>
#include <arisa/composition/gesture_handlers.hpp>
#include <arisa/composition/gesture_layout.hpp>
#include <arisa/composition/gesture_protocol.hpp>
#include <arisa/composition/gesture_state.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace arisa::composition {

template <class First, class Second>
class SequenceGesture
    : public GestureCombinators<SequenceGesture<First, Second>> {
 public:
  enum class Stage : std::uint8_t {
    first,
    second,
  };

  struct Value {
    Stage stage{Stage::first};
    GestureValue<First> first{};
    std::optional<GestureValue<Second>> second;

    friend auto operator==(const Value&, const Value&) -> bool = default;
  };

  using Observer = GestureLayout::Observer<Value>;

  SequenceGesture(First first, Second second)
      : first_(std::move(first)), second_(std::move(second)) {}

  auto on_changed(std::move_only_function<void(const Value&)>
                      action) && -> SequenceGesture {
    handlers_.on_changed(std::move(action));
    return std::move(*this);
  }

  auto on_ended(std::move_only_function<void(const Value&)>
                    action) && -> SequenceGesture {
    handlers_.on_ended(std::move(action));
    return std::move(*this);
  }

  template <class T, class Body>
  auto updating(GestureStateHandle<T> state, Body body) && -> SequenceGesture {
    handlers_.updating(std::move(state), std::move(body));
    return std::move(*this);
  }

  auto first() const noexcept -> const First& { return first_; }

  auto second() const noexcept -> const Second& { return second_; }

  void lower(GestureLayout& into, Observer observer) && {
    auto relay =
        std::make_shared<Relay>(std::move(handlers_), std::move(observer));
    const std::size_t begin = into.size();
    GestureLayout::lower(std::move(first_), into,
                         GestureLayout::Observer<GestureValue<First>>{
                             .changed =
                                 [relay](const GestureValue<First>& value) {
                                   relay->value = Value{.stage = Stage::first,
                                                        .first = value,
                                                        .second = std::nullopt};
                                   relay->forward.changed(relay->value);
                                 },
                             .ended =
                                 [relay](const GestureValue<First>& value) {
                                   relay->value = Value{.stage = Stage::second,
                                                        .first = value,
                                                        .second = std::nullopt};
                                   relay->started = true;
                                   relay->forward.changed(relay->value);
                                 },
                             .cancelled =
                                 [relay] {
                                   if (!relay->started) {
                                     relay->forward.cancelled();
                                     relay->value = Value{};
                                   }
                                 }});
    const std::size_t middle = into.size();
    GestureLayout::lower(std::move(second_), into,
                         GestureLayout::Observer<GestureValue<Second>>{
                             .changed =
                                 [relay](const GestureValue<Second>& value) {
                                   relay->value.stage = Stage::second;
                                   relay->value.second = value;
                                   relay->forward.changed(relay->value);
                                 },
                             .ended =
                                 [relay](const GestureValue<Second>& value) {
                                   relay->value.stage = Stage::second;
                                   relay->value.second = value;
                                   relay->started = false;
                                   relay->forward.ended(relay->value);
                                   relay->value = Value{};
                                 },
                             .cancelled =
                                 [relay] {
                                   if (relay->started) {
                                     relay->started = false;
                                     relay->forward.cancelled();
                                     relay->value = Value{};
                                   }
                                 }});
    into.branch(GestureLayout::Relation::sequenced, begin, middle);
  }

 private:
  struct Relay {
    Relay(GestureHandlers<Value> handlers, Observer outer) noexcept
        : forward(std::move(handlers), std::move(outer)) {}

    GestureRelay<Value> forward;
    Value value;
    bool started{false};
  };

  First first_;
  Second second_;
  GestureHandlers<Value> handlers_;
};

}
