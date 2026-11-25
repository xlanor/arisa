#pragma once

#include <arisa/composition/geometry_proxy.hpp>

#include <concepts>
#include <cstdlib>
#include <functional>
#include <optional>
#include <utility>

namespace arisa::composition {

class GeometryObserver {
 public:
  template <std::equality_comparable T>
  class Typed;

  template <std::equality_comparable T>
  class Changing;

  virtual ~GeometryObserver() = default;

  virtual void notify(const GeometryProxy& proxy) = 0;

  virtual auto kind() const noexcept -> const void* { return nullptr; }

  virtual void adopt(const GeometryObserver&) {}
};

template <std::equality_comparable T>
class GeometryObserver::Typed final : public GeometryObserver {
 public:
  using Transform = std::move_only_function<T(const GeometryProxy&) const>;
  using Action = std::move_only_function<void(const T&)>;

  Typed(Transform transform, Action action) noexcept
      : transform_(std::move(transform)), action_(std::move(action)) {}

  void notify(const GeometryProxy& proxy) override {
    T value = transform_(proxy);
    if (last_.has_value() && same(*last_, value)) {
      return;
    }
    last_ = value;
    action_(value);
  }

 private:
  static constexpr double tolerance = 0.01;

  static auto same(const T& left, const T& right) -> bool {
    if constexpr (std::floating_point<T>) {
      return std::abs(static_cast<double>(left) - static_cast<double>(right)) <
             tolerance;
    } else {
      return left == right;
    }
  }

  Transform transform_;
  Action action_;
  std::optional<T> last_;
};

template <std::equality_comparable T>
class GeometryObserver::Changing final : public GeometryObserver {
 public:
  using Transform = std::move_only_function<T(const GeometryProxy&) const>;
  using Action = std::move_only_function<void(const T&, const T&)>;

  Changing(Transform transform, Action action) noexcept
      : transform_(std::move(transform)), action_(std::move(action)) {}

  void notify(const GeometryProxy& proxy) override {
    T value = transform_(proxy);
    if (!last_.has_value()) {
      last_ = value;
      action_(*last_, value);
      return;
    }
    if (same(*last_, value)) {
      return;
    }
    T old = std::exchange(*last_, value);
    action_(old, *last_);
  }

  auto kind() const noexcept -> const void* override { return &tag; }

  void adopt(const GeometryObserver& previous) override {
    if (previous.kind() == kind() && !last_.has_value()) {
      last_ = static_cast<const Changing&>(previous).last_;
    }
  }

 private:
  static constexpr char tag{};

  static constexpr double tolerance = 0.01;

  static auto same(const T& left, const T& right) -> bool {
    if constexpr (std::floating_point<T>) {
      return std::abs(static_cast<double>(left) - static_cast<double>(right)) <
             tolerance;
    } else {
      return left == right;
    }
  }

  Transform transform_;
  Action action_;
  std::optional<T> last_;
};

}
