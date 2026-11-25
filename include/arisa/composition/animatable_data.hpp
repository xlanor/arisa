#pragma once

#include <arisa/motion/animatable_traits.hpp>
#include <arisa/motion/animatable_vector.hpp>

#include <concepts>
#include <type_traits>

namespace arisa::composition {

class Context;

template <class T>
concept AnimatableView = requires(const T& view, T& changed) {
  typename T::AnimatableData;
  requires motion::Animatable<typename T::AnimatableData>;
  { view.animatable_data() } -> std::convertible_to<typename T::AnimatableData>;
  changed.set_animatable_data(view.animatable_data());
};

class AnimatableData {
 public:
  template <AnimatableView T>
  static auto presented(Context& context, const T& view) -> T {
    using Data = typename T::AnimatableData;
    const motion::AnimatableVector shown =
        present(context,
                motion::AnimatableTraits<Data>::vector(view.animatable_data()));
    T copy = view;
    copy.set_animatable_data(motion::AnimatableTraits<Data>::value(shown));
    return copy;
  }

 private:
  static auto present(Context& context, const motion::AnimatableVector& target)
      -> motion::AnimatableVector;
};

}
