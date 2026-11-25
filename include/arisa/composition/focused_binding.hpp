#pragma once

#include <arisa/composition/binding.hpp>

#include <optional>
#include <utility>

namespace arisa::composition {

template <class T>
class FocusedBinding {
 public:
  explicit FocusedBinding(std::optional<Binding<T>> binding) noexcept
      : binding_(std::move(binding)) {}

  auto wrapped_value() const -> std::optional<T> {
    if (!binding_.has_value()) {
      return std::nullopt;
    }
    return binding_->get();
  }

  void set(std::optional<T> value) const {
    if (binding_.has_value() && value.has_value()) {
      binding_->set(std::move(*value));
    }
  }

  auto projected_value() const -> Binding<std::optional<T>> {
    return Binding<std::optional<T>>{
        [held = binding_]() -> std::optional<T> {
          if (!held.has_value()) {
            return std::nullopt;
          }
          return held->get();
        },
        [held = binding_](std::optional<T> value) {
          if (held.has_value() && value.has_value()) {
            held->set(std::move(*value));
          }
        }};
  }

  auto bound() const noexcept -> bool { return binding_.has_value(); }

 private:
  std::optional<Binding<T>> binding_;
};

}
