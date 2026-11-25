#pragma once

#include <arisa/composition/identity.hpp>

#include <string_view>
#include <utility>

namespace arisa::composition {

template <class T>
class LayoutValueKey {
 public:
  using Value = T;

  constexpr LayoutValueKey(std::string_view name, T fallback)
      : name_(name), fallback_(std::move(fallback)) {}

  LayoutValueKey(const LayoutValueKey&) = delete;
  auto operator=(const LayoutValueKey&) -> LayoutValueKey& = delete;
  LayoutValueKey(LayoutValueKey&&) = delete;
  auto operator=(LayoutValueKey&&) -> LayoutValueKey& = delete;
  ~LayoutValueKey() = default;

  constexpr auto name() const noexcept -> std::string_view { return name_; }

  constexpr auto fallback() const noexcept -> const T& { return fallback_; }

  auto identity() const noexcept -> Identity { return Identity{this}; }

 private:
  std::string_view name_;
  T fallback_;
};

}
