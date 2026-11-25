#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/motion/unit_point.hpp>

#include <optional>
#include <utility>

namespace arisa::composition {

class ScrollPositionBinding {
 public:
  template <class T>
  class Typed;

  explicit ScrollPositionBinding(
      std::optional<motion::UnitPoint> anchor) noexcept
      : anchor_(anchor) {}

  virtual ~ScrollPositionBinding() = default;

  auto anchor() const noexcept -> std::optional<motion::UnitPoint> {
    return anchor_;
  }

  virtual auto read() const -> std::optional<ElementKey> = 0;

  virtual void write(const std::optional<ElementKey>& id) = 0;

 private:
  std::optional<motion::UnitPoint> anchor_;
};

template <class T>
class ScrollPositionBinding::Typed final : public ScrollPositionBinding {
 public:
  Typed(Binding<std::optional<T>> state,
        std::optional<motion::UnitPoint> anchor) noexcept
      : ScrollPositionBinding(anchor), state_(std::move(state)) {}

  auto read() const -> std::optional<ElementKey> override {
    const std::optional<T> value = state_.get();
    if (!value.has_value()) {
      return std::nullopt;
    }
    return ElementKey{*value};
  }

  void write(const std::optional<ElementKey>& id) override {
    if (!id.has_value()) {
      state_.set(std::nullopt);
      return;
    }
    if (const auto value = id->template as<T>()) {
      state_.set(*value);
    }
  }

 private:
  Binding<std::optional<T>> state_;
};

}
