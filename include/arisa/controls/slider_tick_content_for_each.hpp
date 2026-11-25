#pragma once

#include <arisa/controls/slider_tick.hpp>
#include <arisa/controls/slider_tick_content.hpp>

#include <concepts>
#include <functional>
#include <ranges>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::controls {

class SliderTickContentForEach {
 public:
  template <std::ranges::input_range Data, class Build>
    requires std::convertible_to<
        std::invoke_result_t<Build&, std::ranges::range_reference_t<Data>>,
        SliderTickContent>
  SliderTickContentForEach(Data&& data, Build build) {
    for (auto&& element : data) {
      append(SliderTickContent{std::invoke(build, element)});
    }
  }

  template <std::ranges::input_range Data, class Identify, class Build>
    requires std::convertible_to<
                 std::invoke_result_t<Build&,
                                      std::ranges::range_reference_t<Data>>,
                 SliderTickContent> &&
             std::invocable<Identify&, std::ranges::range_reference_t<Data>>
  SliderTickContentForEach(Data&& data, Identify id, Build build) {
    using Key = std::remove_cvref_t<
        std::invoke_result_t<Identify&, std::ranges::range_reference_t<Data>>>;
    std::set<Key> seen;
    for (auto&& element : data) {
      if (!seen.insert(std::invoke(id, element)).second) {
        continue;
      }
      append(SliderTickContent{std::invoke(build, element)});
    }
  }

  auto body() const -> std::vector<SliderTick> { return ticks_; }

 private:
  void append(const SliderTickContent& content) {
    for (const SliderTick& tick : content.ticks()) {
      ticks_.push_back(tick);
    }
  }

  std::vector<SliderTick> ticks_;
};

}
