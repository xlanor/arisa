#include <interaction/velocity_tracker.hpp>

#include <algorithm>

namespace arisa::interaction {

void VelocityTracker::reset(composition::Point point,
                            std::chrono::nanoseconds now) {
  samples_.clear();
  samples_.push_back(Sample{.point = point, .time = now});
}

void VelocityTracker::add(composition::Point point,
                          std::chrono::nanoseconds now) {
  if (!samples_.empty() && samples_.back().time >= now) {
    samples_.back().point = point;
  } else {
    samples_.push_back(Sample{.point = point, .time = now});
  }
  std::erase_if(samples_, [now](const Sample& sample) {
    return now - sample.time > window;
  });
}

auto VelocityTracker::velocity(std::chrono::nanoseconds now) const
    -> composition::Size {
  if (samples_.size() < 2 || now - samples_.back().time > window) {
    return composition::Size{};
  }
  const Sample& first = samples_.front();
  const Sample& last = samples_.back();
  const double seconds =
      std::chrono::duration<double>(last.time - first.time).count();
  if (seconds <= 0.0) {
    return composition::Size{};
  }
  return composition::Size{
      .width = static_cast<float>((last.point.x - first.point.x) / seconds),
      .height = static_cast<float>((last.point.y - first.point.y) / seconds)};
}

}
