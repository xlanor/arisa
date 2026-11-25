#include <arisa/controls/slider_tick_content.hpp>
#include <arisa/controls/slider_tick_content_for_each.hpp>
#include <arisa/controls/tuple_slider_tick_content.hpp>

#include <algorithm>
#include <functional>
#include <utility>

namespace arisa::controls {

SliderTickContent::SliderTickContent(SliderTick tick)
    : ticks_{std::move(tick)} {}

SliderTickContent::SliderTickContent(std::vector<SliderTick> ticks)
    : ticks_(std::move(ticks)) {}

SliderTickContent::SliderTickContent(const TupleSliderTickContent& ticks)
    : ticks_(ticks.body()) {}

SliderTickContent::SliderTickContent(const SliderTickContentForEach& ticks)
    : ticks_(ticks.body()) {}

auto SliderTickContent::sorted() const -> std::vector<SliderTick> {
  std::vector<SliderTick> ordered = ticks_;
  std::ranges::stable_sort(ordered, std::less<>{});
  const auto [first, last] = std::ranges::unique(ordered);
  ordered.erase(first, last);
  return ordered;
}

}
