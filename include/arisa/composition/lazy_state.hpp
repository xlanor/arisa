#pragma once

#include <arisa/composition/identity.hpp>

#include <concepts>

namespace arisa::composition {

template <class T>
class LazyState {
 public:
  using Make = T (*)();

  constexpr LazyState() noexcept
    requires std::default_initializable<T>
      : make_(nullptr) {}

  constexpr explicit LazyState(Make make) noexcept : make_(make) {}

  LazyState(const LazyState&) = delete;
  auto operator=(const LazyState&) -> LazyState& = delete;
  LazyState(LazyState&&) = delete;
  auto operator=(LazyState&&) -> LazyState& = delete;
  ~LazyState() = default;

  auto initial_value() const -> T {
    if constexpr (std::default_initializable<T>) {
      if (make_ == nullptr) {
        return T{};
      }
    }
    return make_();
  }

  auto identity() const noexcept -> Identity { return Identity{this}; }

 private:
  Make make_;
};

}
