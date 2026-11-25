#pragma once

#include <arisa/composition/element.hpp>

#include <compare>
#include <concepts>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::controls {

class SliderTick {
 public:
  using Label = std::function<composition::Element()>;

  class ID {
   public:
    explicit ID(double value) noexcept : value_(value) {}

    auto value() const noexcept -> double { return value_; }

    friend auto operator==(const ID&, const ID&) -> bool = default;

   private:
    double value_;
  };

  template <std::floating_point T>
  explicit SliderTick(T value) : value_(static_cast<double>(value)) {}

  template <std::floating_point T>
  SliderTick(T value, Label label)
      : value_(static_cast<double>(value)), label_(std::move(label)) {}

  template <std::floating_point T>
  SliderTick(std::string title, T value)
      : value_(static_cast<double>(value)), title_(std::move(title)) {}

  auto value() const noexcept -> double { return value_; }

  auto id() const noexcept -> ID { return ID{value_}; }

  auto title() const noexcept -> const std::optional<std::string>& {
    return title_;
  }

  auto label() const noexcept -> const Label& { return label_; }

  auto labelled() const -> std::optional<composition::Element>;

  auto body() const -> std::vector<SliderTick>;

  friend auto operator==(const SliderTick& left, const SliderTick& right)
      -> bool {
    return left.value_ == right.value_;
  }

  friend auto operator<(const SliderTick& left, const SliderTick& right)
      -> bool {
    return left.value_ < right.value_;
  }

 private:
  double value_;
  std::optional<std::string> title_;
  Label label_;
};

}
