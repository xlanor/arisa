#pragma once

#include <arisa/composition/element_type.hpp>

#include <concepts>
#include <memory>
#include <optional>
#include <utility>

namespace arisa::composition {

class TagValue {
 public:
  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  class Typed;

  virtual ~TagValue() = default;

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  static auto of(T value) -> std::shared_ptr<const TagValue>;

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  static auto of(T value, bool include_optional)
      -> std::shared_ptr<const TagValue>;

  virtual auto kind() const noexcept -> ElementType = 0;

  virtual auto alternate() const noexcept -> const TagValue* { return nullptr; }

  virtual auto equals(const TagValue& other) const -> bool = 0;

  template <class T>
  auto get() const noexcept -> const T*;
};

template <std::equality_comparable T>
  requires std::copy_constructible<T>
class TagValue::Typed final : public TagValue {
 public:
  explicit Typed(T value) : value_(std::move(value)) {}

  Typed(T value, std::shared_ptr<const TagValue> optional)
      : value_(std::move(value)), optional_(std::move(optional)) {}

  auto alternate() const noexcept -> const TagValue* override {
    return optional_.get();
  }

  auto kind() const noexcept -> ElementType override {
    return ElementType::of<Typed>();
  }

  auto equals(const TagValue& other) const -> bool override {
    return other.kind() == kind() &&
           static_cast<const Typed&>(other).value_ == value_;
  }

  auto value() const noexcept -> const T& { return value_; }

 private:
  T value_;
  std::shared_ptr<const TagValue> optional_;
};

template <std::equality_comparable T>
  requires std::copy_constructible<T>
auto TagValue::of(T value) -> std::shared_ptr<const TagValue> {
  return std::make_shared<const Typed<T>>(std::move(value));
}

template <std::equality_comparable T>
  requires std::copy_constructible<T>
auto TagValue::of(T value, bool include_optional)
    -> std::shared_ptr<const TagValue> {
  if (!include_optional) {
    return of(std::move(value));
  }
  std::shared_ptr<const TagValue> optional =
      std::make_shared<const Typed<std::optional<T>>>(std::optional<T>{value});
  return std::make_shared<const Typed<T>>(std::move(value),
                                          std::move(optional));
}

template <class T>
auto TagValue::get() const noexcept -> const T* {
  if (kind() != ElementType::of<Typed<T>>()) {
    if (const TagValue* other = alternate();
        other != nullptr && other->kind() == ElementType::of<Typed<T>>()) {
      return &static_cast<const Typed<T>&>(*other).value();
    }
    return nullptr;
  }
  return &static_cast<const Typed<T>&>(*this).value();
}

}
