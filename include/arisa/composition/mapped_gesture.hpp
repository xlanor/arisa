#pragma once

#include <arisa/composition/gesture.hpp>
#include <arisa/composition/gesture_combinators.hpp>
#include <arisa/composition/gesture_handlers.hpp>
#include <arisa/composition/gesture_layout.hpp>
#include <arisa/composition/gesture_protocol.hpp>
#include <arisa/composition/gesture_state.hpp>

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::composition {

template <class Base, class Transform>
class MappedGesture
    : public GestureCombinators<MappedGesture<Base, Transform>> {
 public:
  using Value = std::remove_cvref_t<
      std::invoke_result_t<const Transform&, const GestureValue<Base>&>>;
  using Observer = GestureLayout::Observer<Value>;

  MappedGesture(Base base, Transform transform)
      : base_(std::move(base)), transform_(std::move(transform)) {}

  auto on_changed(
      std::move_only_function<void(const Value&)> action) && -> MappedGesture {
    handlers_.on_changed(std::move(action));
    return std::move(*this);
  }

  auto on_ended(
      std::move_only_function<void(const Value&)> action) && -> MappedGesture {
    handlers_.on_ended(std::move(action));
    return std::move(*this);
  }

  template <class T, class Body>
  auto updating(GestureStateHandle<T> state, Body body) && -> MappedGesture {
    handlers_.updating(std::move(state), std::move(body));
    return std::move(*this);
  }

  auto base() const noexcept -> const Base& { return base_; }

  void lower(GestureLayout& into, Observer observer) && {
    auto relay = std::make_shared<GestureRelay<Value>>(std::move(handlers_),
                                                       std::move(observer));
    auto transform = std::make_shared<const Transform>(std::move(transform_));
    GestureLayout::lower(
        std::move(base_), into,
        GestureLayout::Observer<GestureValue<Base>>{
            .changed =
                [relay, transform](const GestureValue<Base>& value) {
                  relay->changed(std::invoke(*transform, value));
                },
            .ended =
                [relay, transform](const GestureValue<Base>& value) {
                  relay->ended(std::invoke(*transform, value));
                },
            .cancelled = [relay] { relay->cancelled(); }});
  }

 private:
  Base base_;
  Transform transform_;
  GestureHandlers<Value> handlers_;
};

}
