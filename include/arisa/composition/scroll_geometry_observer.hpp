#pragma once

#include <arisa/composition/scroll_geometry.hpp>

#include <concepts>
#include <functional>
#include <memory>
#include <utility>

namespace arisa::composition {

class ScrollGeometryObserver {
 public:
  template <std::equality_comparable T>
  class Typed;

  virtual ~ScrollGeometryObserver() = default;

  virtual auto differs(const ScrollGeometry& before,
                       const ScrollGeometry& after) const -> bool = 0;

  virtual void notify(const ScrollGeometry& before,
                      const ScrollGeometry& after) = 0;
};

template <std::equality_comparable T>
class ScrollGeometryObserver::Typed final : public ScrollGeometryObserver {
 public:
  using Transform = std::move_only_function<T(const ScrollGeometry&) const>;
  using Action = std::move_only_function<void(const T&, const T&)>;

  Typed(Transform transform, Action action) noexcept
      : transform_(std::move(transform)), action_(std::move(action)) {}

  auto differs(const ScrollGeometry& before, const ScrollGeometry& after) const
      -> bool override {
    return !(transform_(before) == transform_(after));
  }

  void notify(const ScrollGeometry& before,
              const ScrollGeometry& after) override {
    action_(transform_(before), transform_(after));
  }

 private:
  Transform transform_;
  Action action_;
};

}
