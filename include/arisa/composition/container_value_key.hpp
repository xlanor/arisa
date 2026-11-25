#pragma once

#include <arisa/composition/identity.hpp>

#include <string_view>
#include <utility>

namespace arisa::composition {

template <class T>
class ContainerValueKey {
 public:
  using Value = T;

  constexpr ContainerValueKey(std::string_view name, T fallback)
      : name_(name), fallback_(std::move(fallback)) {}

  ContainerValueKey(const ContainerValueKey&) = delete;
  auto operator=(const ContainerValueKey&) -> ContainerValueKey& = delete;
  ContainerValueKey(ContainerValueKey&&) = delete;
  auto operator=(ContainerValueKey&&) -> ContainerValueKey& = delete;
  ~ContainerValueKey() = default;

  constexpr auto name() const noexcept -> std::string_view { return name_; }

  constexpr auto fallback() const noexcept -> const T& { return fallback_; }

  auto identity() const noexcept -> Identity { return Identity{this}; }

 private:
  std::string_view name_;
  T fallback_;
};

}
