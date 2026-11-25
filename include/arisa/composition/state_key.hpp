#pragma once

#include <arisa/composition/identity.hpp>

#include <string_view>
#include <utility>

namespace arisa::composition {

template <class T>
class StateKey {
 public:
  constexpr explicit StateKey(std::string_view name, T initial = T{})
      : name_(name), initial_(std::move(initial)) {}

  StateKey(const StateKey&) = delete;
  auto operator=(const StateKey&) -> StateKey& = delete;
  StateKey(StateKey&&) = delete;
  auto operator=(StateKey&&) -> StateKey& = delete;
  ~StateKey() = default;

  constexpr auto name() const noexcept -> std::string_view { return name_; }

  constexpr auto initial() const noexcept -> const T& { return initial_; }

  auto identity() const noexcept -> Identity { return Identity{this}; }

 private:
  std::string_view name_;
  T initial_;
};

}
