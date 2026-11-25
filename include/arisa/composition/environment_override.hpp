#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/identity.hpp>

#include <memory>
#include <utility>

namespace arisa::composition {

class EnvironmentOverride {
 public:
  class Value {
   public:
    virtual ~Value() = default;

    virtual auto equals(const Value& other) const -> bool = 0;
  };

  template <class T>
  class Typed final : public Value {
   public:
    explicit Typed(T value) : value_(std::move(value)) {}

    auto get() const noexcept -> const T& { return value_; }

    auto equals(const Value& other) const -> bool override {
      return static_cast<const Typed&>(other).value_ == value_;
    }

   private:
    T value_;
  };

  template <class T>
  static auto of(const EnvironmentKey<T>& key, T value) -> EnvironmentOverride {
    return EnvironmentOverride{
        key.identity(), std::make_shared<const Typed<T>>(std::move(value))};
  }

  auto key() const noexcept -> Identity { return key_; }

  auto value() const noexcept -> const Value& { return *value_; }

  auto shared() const noexcept -> const std::shared_ptr<const Value>& {
    return value_;
  }

  auto same_value(const EnvironmentOverride& other) const -> bool {
    return other.key_ == key_ && value_->equals(*other.value_);
  }

 private:
  EnvironmentOverride(Identity key, std::shared_ptr<const Value> value) noexcept
      : key_(key), value_(std::move(value)) {}

  Identity key_;
  std::shared_ptr<const Value> value_;
};

}
