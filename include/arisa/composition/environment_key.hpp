#pragma once

#include <arisa/composition/identity.hpp>

#include <string_view>
#include <utility>

namespace arisa::composition {

template <class T>
class EnvironmentKey {
 public:
  using Merge = T (*)(const T& inherited, const T& declared);

  constexpr EnvironmentKey(std::string_view name, T fallback,
                           Merge combine = nullptr)
      : name_(name), fallback_(std::move(fallback)), merge_(combine) {}

  EnvironmentKey(const EnvironmentKey&) = delete;
  auto operator=(const EnvironmentKey&) -> EnvironmentKey& = delete;
  EnvironmentKey(EnvironmentKey&&) = delete;
  auto operator=(EnvironmentKey&&) -> EnvironmentKey& = delete;
  ~EnvironmentKey() = default;

  constexpr auto name() const noexcept -> std::string_view { return name_; }

  constexpr auto fallback() const noexcept -> const T& { return fallback_; }

  constexpr auto merge() const noexcept -> Merge { return merge_; }

  auto identity() const noexcept -> Identity { return Identity{this}; }

 private:
  std::string_view name_;
  T fallback_;
  Merge merge_;
};

}
