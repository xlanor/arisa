#pragma once

#include <arisa/composition/point.hpp>
#include <arisa/composition/size.hpp>

#include <chrono>
#include <vector>

namespace arisa::interaction {

class VelocityTracker {
 public:
  static constexpr std::chrono::milliseconds window{100};

  void reset(composition::Point point, std::chrono::nanoseconds now);

  void add(composition::Point point, std::chrono::nanoseconds now);

  auto velocity(std::chrono::nanoseconds now) const -> composition::Size;

 private:
  struct Sample {
    composition::Point point{};
    std::chrono::nanoseconds time{0};
  };

  std::vector<Sample> samples_;
};

}
