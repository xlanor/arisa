#pragma once

#include <arisa/composition/gesture.hpp>
#include <arisa/composition/gesture_combinators.hpp>
#include <arisa/composition/gesture_handlers.hpp>
#include <arisa/composition/gesture_layout.hpp>
#include <arisa/composition/gesture_protocol.hpp>
#include <arisa/composition/gesture_state.hpp>

#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::composition {

template <class V>
class AnyGesture : public GestureCombinators<AnyGesture<V>> {
 public:
  using Value = V;
  using Observer = GestureLayout::Observer<V>;

  template <GestureProtocol G>
    requires(std::same_as<GestureValue<G>, V> &&
             !std::same_as<std::remove_cvref_t<G>, AnyGesture>)
  explicit AnyGesture(G gesture)
      : lower_([held = std::move(gesture)](GestureLayout& into,
                                           Observer observer) mutable {
          GestureLayout::lower(std::move(held), into, std::move(observer));
        }) {}

  auto on_changed(
      std::move_only_function<void(const Value&)> action) && -> AnyGesture {
    handlers_.on_changed(std::move(action));
    return std::move(*this);
  }

  auto on_ended(
      std::move_only_function<void(const Value&)> action) && -> AnyGesture {
    handlers_.on_ended(std::move(action));
    return std::move(*this);
  }

  template <class T, class Body>
  auto updating(GestureStateHandle<T> state, Body body) && -> AnyGesture {
    handlers_.updating(std::move(state), std::move(body));
    return std::move(*this);
  }

  void lower(GestureLayout& into, Observer observer) && {
    auto relay = std::make_shared<GestureRelay<V>>(std::move(handlers_),
                                                   std::move(observer));
    lower_(into, GestureRelay<V>::observer(std::move(relay)));
  }

 private:
  std::move_only_function<void(GestureLayout&, Observer)> lower_;
  GestureHandlers<V> handlers_;
};

}
