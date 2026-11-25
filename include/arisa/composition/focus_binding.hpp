#pragma once

#include <arisa/composition/identity.hpp>
#include <arisa/composition/state_handle.hpp>

#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>

namespace arisa::composition {

template <class T>
class FocusBinding {
 public:
  using Value =
      std::conditional_t<std::same_as<T, bool>, bool, std::optional<T>>;

  explicit FocusBinding(StateHandle<Value> handle) noexcept
      : handle_(std::move(handle)) {}

  auto get() const noexcept -> const Value& { return handle_.get(); }

  void set(Value value) const { handle_.set(std::move(value)); }

  auto identity() const noexcept -> Identity { return handle_.identity(); }

 private:
  StateHandle<Value> handle_;
};

}
