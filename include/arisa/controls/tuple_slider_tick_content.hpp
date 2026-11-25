#pragma once

#include <arisa/controls/slider_tick.hpp>

#include <concepts>
#include <span>
#include <utility>
#include <vector>

namespace arisa::controls {

class TupleSliderTickContent {
 public:
  template <class... Ticks>
    requires(std::convertible_to<Ticks, SliderTick> && ...)
  explicit TupleSliderTickContent(Ticks... ticks) {
    value_.reserve(sizeof...(Ticks));
    (value_.push_back(SliderTick{std::move(ticks)}), ...);
  }

  auto value() const noexcept -> std::span<const SliderTick> { return value_; }

  auto body() const -> std::vector<SliderTick> { return value_; }

 private:
  std::vector<SliderTick> value_;
};

}
