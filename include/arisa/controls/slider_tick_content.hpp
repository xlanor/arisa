#pragma once

#include <arisa/controls/slider_tick.hpp>

#include <span>
#include <utility>
#include <vector>

namespace arisa::controls {

class TupleSliderTickContent;
class SliderTickContentForEach;

class SliderTickContent {
 public:
  SliderTickContent() = default;

  SliderTickContent(SliderTick tick);

  SliderTickContent(std::vector<SliderTick> ticks);

  SliderTickContent(const TupleSliderTickContent& ticks);

  SliderTickContent(const SliderTickContentForEach& ticks);

  auto ticks() const noexcept -> std::span<const SliderTick> { return ticks_; }

  auto sorted() const -> std::vector<SliderTick>;

 private:
  std::vector<SliderTick> ticks_;
};

}
