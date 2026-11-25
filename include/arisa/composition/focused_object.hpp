#pragma once

#include <arisa/composition/focused_value_key.hpp>
#include <arisa/composition/observable.hpp>

namespace arisa::composition {

template <class T>
class FocusedObject {
 public:
  explicit FocusedObject(Observable<T>& object) noexcept : object_(&object) {}

  auto get() const noexcept -> Observable<T>& { return *object_; }

  friend auto operator==(const FocusedObject&, const FocusedObject&)
      -> bool = default;

 private:
  Observable<T>* object_;
};

template <class T>
inline const FocusedValueKey<FocusedObject<T>> focused_object_key{
    "arisa.focused-object"};

}
