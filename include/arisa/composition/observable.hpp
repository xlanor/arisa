#pragma once

#include <arisa/composition/observable_core.hpp>

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::composition {

template <class T>
class Observable {
 public:
  explicit Observable(T initial = T{}) : value_(std::move(initial)) {}

  auto get() const noexcept -> const T& { return value_; }

  void set(T value) {
    if constexpr (std::equality_comparable<T>) {
      if (value_ == value) {
        return;
      }
    }
    value_ = std::move(value);
    core_->publish();
  }

  template <std::invocable<T&> Mutation>
  void update(Mutation&& mutation) {
    std::invoke(std::forward<Mutation>(mutation), value_);
    core_->publish();
  }

  template <std::invocable<T&> Mutation>
  void update(std::uintptr_t channel, Mutation&& mutation) {
    std::invoke(std::forward<Mutation>(mutation), value_);
    core_->publish(channel);
  }

  template <class Field, class Whole = T>
    requires std::is_class_v<Whole>
  void write(Field Whole::* member, Field value) {
    if constexpr (std::equality_comparable<Field>) {
      if (value_.*member == value) {
        return;
      }
    }
    value_.*member = std::move(value);
    core_->publish(channel_of(member));
  }

  template <class Field, class Whole = T>
    requires std::is_class_v<Whole>
  static auto channel_of(Field Whole::* member) noexcept -> std::uintptr_t {
    static_assert(sizeof(member) == sizeof(std::uintptr_t));
    return std::bit_cast<std::uintptr_t>(member);
  }

  auto version() const noexcept -> ObservableVersion {
    return core_->version();
  }

  auto subscribe(ObservableCore::Listener listener) const -> Subscription {
    return core_->subscribe(std::move(listener));
  }

  auto subscribers() const noexcept -> std::size_t {
    return core_->subscribers();
  }

 private:
  friend class Context;

  T value_;
  std::shared_ptr<ObservableCore> core_{std::make_shared<ObservableCore>()};
};

}
