#pragma once

#include <utility>

namespace arisa::composition {

template <class First, class Second>
class SimultaneousGesture;

template <class First, class Second>
class SequenceGesture;

template <class First, class Second>
class ExclusiveGesture;

template <class Base, class Transform>
class MappedGesture;

template <class Self>
class GestureCombinators {
 public:
  template <class Other>
  auto simultaneously(Other other) && -> SimultaneousGesture<Self, Other> {
    return SimultaneousGesture<Self, Other>{
        std::move(static_cast<Self&>(*this)), std::move(other)};
  }

  template <class Other>
  auto sequenced(Other before) && -> SequenceGesture<Self, Other> {
    return SequenceGesture<Self, Other>{std::move(static_cast<Self&>(*this)),
                                        std::move(before)};
  }

  template <class Other>
  auto exclusively(Other before) && -> ExclusiveGesture<Self, Other> {
    return ExclusiveGesture<Self, Other>{std::move(static_cast<Self&>(*this)),
                                         std::move(before)};
  }

  template <class Transform>
  auto map(Transform transform) && -> MappedGesture<Self, Transform> {
    return MappedGesture<Self, Transform>{std::move(static_cast<Self&>(*this)),
                                          std::move(transform)};
  }
};

}
