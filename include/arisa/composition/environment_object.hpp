#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/observable.hpp>

#include <optional>

namespace arisa::composition {

template <class T>
class EnvironmentObject {
 public:
  explicit EnvironmentObject(Observable<T>& object) noexcept
      : object_(&object) {}

  auto get() const noexcept -> Observable<T>& { return *object_; }

  friend auto operator==(const EnvironmentObject&, const EnvironmentObject&)
      -> bool = default;

 private:
  Observable<T>* object_;
};

template <class T>
inline const EnvironmentKey<std::optional<EnvironmentObject<T>>>
    environment_object_key{"arisa.environment-object", std::nullopt};

}
