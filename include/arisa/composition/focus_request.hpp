#pragma once

#include <arisa/composition/element_type.hpp>
#include <arisa/composition/focus_binding.hpp>

#include <concepts>
#include <memory>
#include <optional>
#include <utility>

namespace arisa::composition {

class FocusRequest {
 public:
  template <class T>
  class Typed;

  virtual ~FocusRequest() = default;

  virtual auto kind() const noexcept -> ElementType = 0;

  virtual auto requested() const -> bool = 0;

  virtual void grant() const = 0;

  virtual void release() const = 0;

  virtual auto same_target(const FocusRequest& other) const -> bool = 0;

  static auto of(FocusBinding<bool> binding, bool value = true)
      -> std::unique_ptr<FocusRequest>;

  template <std::equality_comparable T>
    requires(!std::same_as<T, bool>)
  static auto of(FocusBinding<T> binding, T value)
      -> std::unique_ptr<FocusRequest>;
};

template <class T>
class FocusRequest::Typed final : public FocusRequest {
 public:
  using Target = std::conditional_t<std::same_as<T, bool>, bool, T>;

  Typed(FocusBinding<T> binding, Target target)
      : binding_(std::move(binding)), target_(std::move(target)) {}

  auto kind() const noexcept -> ElementType override {
    return ElementType::of<Typed>();
  }

  auto requested() const -> bool override {
    if constexpr (std::same_as<T, bool>) {
      return binding_.get() == target_;
    } else {
      return binding_.get().has_value() && *binding_.get() == target_;
    }
  }

  void grant() const override { binding_.set(target_); }

  void release() const override {
    if (!requested()) {
      return;
    }
    if constexpr (std::same_as<T, bool>) {
      binding_.set(!target_);
    } else {
      binding_.set(std::nullopt);
    }
  }

  auto same_target(const FocusRequest& other) const -> bool override {
    if (other.kind() != kind()) {
      return false;
    }
    const auto& typed = static_cast<const Typed&>(other);
    return typed.binding_.identity() == binding_.identity() &&
           typed.target_ == target_;
  }

 private:
  FocusBinding<T> binding_;
  Target target_;
};

inline auto FocusRequest::of(FocusBinding<bool> binding, bool value)
    -> std::unique_ptr<FocusRequest> {
  return std::make_unique<Typed<bool>>(std::move(binding), value);
}

template <std::equality_comparable T>
  requires(!std::same_as<T, bool>)
auto FocusRequest::of(FocusBinding<T> binding, T value)
    -> std::unique_ptr<FocusRequest> {
  return std::make_unique<Typed<T>>(std::move(binding), std::move(value));
}

}
