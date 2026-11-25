#pragma once

#include <utility>

namespace arisa::composition {

class StateSlot {
 public:
  template <class T>
  class Typed;

  virtual ~StateSlot() = default;
};

template <class T>
class StateSlot::Typed final : public StateSlot {
 public:
  explicit Typed(T value) : value_(std::move(value)) {}

  auto value() noexcept -> T& { return value_; }

  auto value() const noexcept -> const T& { return value_; }

 private:
  T value_;
};

}
