#pragma once

#include <arisa/composition/focus_binding.hpp>
#include <arisa/composition/state_key.hpp>

#include <string_view>

namespace arisa::composition {

template <class T>
class FocusState {
 public:
  using Binding = FocusBinding<T>;
  using Value = typename Binding::Value;

  constexpr explicit FocusState(std::string_view name) : key_(name, Value{}) {}

  FocusState(const FocusState&) = delete;
  auto operator=(const FocusState&) -> FocusState& = delete;
  FocusState(FocusState&&) = delete;
  auto operator=(FocusState&&) -> FocusState& = delete;
  ~FocusState() = default;

  constexpr auto key() const noexcept -> const StateKey<Value>& { return key_; }

 private:
  StateKey<Value> key_;
};

}
