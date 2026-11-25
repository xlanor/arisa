#pragma once

#include <type_traits>
#include <utility>

namespace arisa::composition {

template <class G>
concept DeclaresGestureValue = requires { typename G::Value; };

template <class G>
concept ComposedGesture =
    !DeclaresGestureValue<G> && requires(const G& gesture) { gesture.body(); };

template <class G>
struct GestureValueOf {
  using type = typename G::Value;
};

template <ComposedGesture G>
struct GestureValueOf<G> {
  using type = typename GestureValueOf<
      std::remove_cvref_t<decltype(std::declval<const G&>().body())>>::type;
};

template <class G>
using GestureValue = typename GestureValueOf<std::remove_cvref_t<G>>::type;

template <class G>
concept GestureProtocol = DeclaresGestureValue<std::remove_cvref_t<G>> ||
                          ComposedGesture<std::remove_cvref_t<G>>;

}
