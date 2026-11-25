#pragma once

#include <arisa/composition/element_type.hpp>

#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::composition {

class ChangeObserver {
 public:
  template <class T>
  class Typed;

  virtual ~ChangeObserver() = default;

  virtual auto kind() const noexcept -> ElementType = 0;

  virtual auto initial() const noexcept -> bool = 0;

  virtual auto differs(const ChangeObserver& previous) const -> bool = 0;

  virtual void notify(const ChangeObserver& previous) = 0;

  template <std::equality_comparable T, class Action>
    requires std::copy_constructible<T> &&
             (std::invocable<Action&, const T&, const T&> ||
              std::invocable<Action&>)
  static auto of(T value, bool initial, Action action)
      -> std::unique_ptr<ChangeObserver>;
};

template <class T>
class ChangeObserver::Typed final : public ChangeObserver {
 public:
  using Action = std::move_only_function<void(const T&, const T&)>;

  Typed(T value, bool initial, Action action)
      : value_(std::move(value)),
        initial_(initial),
        action_(std::move(action)) {}

  auto kind() const noexcept -> ElementType override {
    return ElementType::of<Typed>();
  }

  auto initial() const noexcept -> bool override { return initial_; }

  auto differs(const ChangeObserver& previous) const -> bool override {
    return previous.kind() == kind() &&
           !(static_cast<const Typed&>(previous).value_ == value_);
  }

  void notify(const ChangeObserver& previous) override {
    if (previous.kind() == kind() && action_) {
      action_(static_cast<const Typed&>(previous).value_, value_);
    }
  }

 private:
  T value_;
  bool initial_;
  Action action_;
};

template <std::equality_comparable T, class Action>
  requires std::copy_constructible<T> &&
           (std::invocable<Action&, const T&, const T&> ||
            std::invocable<Action&>)
auto ChangeObserver::of(T value, bool initial, Action action)
    -> std::unique_ptr<ChangeObserver> {
  if constexpr (std::invocable<Action&, const T&, const T&>) {
    return std::make_unique<Typed<T>>(std::move(value), initial,
                                      std::move(action));
  } else {
    return std::make_unique<Typed<T>>(
        std::move(value), initial,
        [held = std::move(action)](const T&, const T&) mutable { held(); });
  }
}

}
