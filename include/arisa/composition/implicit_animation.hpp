#pragma once

#include <arisa/composition/element_type.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/transaction.hpp>

#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace arisa::composition {

class ImplicitAnimation {
 public:
  class Value {
   public:
    virtual ~Value() = default;

    virtual auto kind() const noexcept -> ElementType = 0;

    virtual auto equals(const Value& other) const -> bool = 0;
  };

  template <class T>
  class Typed final : public Value {
   public:
    explicit Typed(T value) : value_(std::move(value)) {}

    auto kind() const noexcept -> ElementType override {
      return ElementType::of<Typed>();
    }

    auto equals(const Value& other) const -> bool override {
      return other.kind() == kind() &&
             static_cast<const Typed&>(other).value_ == value_;
    }

   private:
    T value_;
  };

  using Edit = std::move_only_function<void(motion::Transaction&)>;

  template <std::equality_comparable T>
  static auto of(std::optional<motion::Animation> animation, T value)
      -> ImplicitAnimation {
    return ImplicitAnimation{std::move(animation),
                             std::make_unique<Typed<T>>(std::move(value))};
  }

  template <std::equality_comparable T>
  static auto edited(T value, Edit edit) -> ImplicitAnimation {
    ImplicitAnimation scoped{std::nullopt,
                             std::make_unique<Typed<T>>(std::move(value))};
    scoped.edit_ = std::move(edit);
    return scoped;
  }

  void apply(motion::Transaction& transaction) {
    if (edit_) {
      edit_(transaction);
      return;
    }
    if (!transaction.disables_animations()) {
      transaction.set_animation(animation_);
    }
  }

  auto animation() const noexcept -> const std::optional<motion::Animation>& {
    return animation_;
  }

  auto same_value(const ImplicitAnimation& other) const -> bool {
    return value_->equals(*other.value_);
  }

 private:
  ImplicitAnimation(std::optional<motion::Animation> animation,
                    std::unique_ptr<Value> value) noexcept
      : animation_(std::move(animation)), value_(std::move(value)) {}

  std::optional<motion::Animation> animation_;
  std::unique_ptr<Value> value_;
  Edit edit_;
};

}
