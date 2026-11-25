#include <frame_meter.hpp>

namespace arisa::examples {

void FrameMeter::record(const runtime::FrameTimings& timings,
                        std::chrono::nanoseconds now) {
  if (timings.presented) {
    ++presented_;
    if (own_frame_) {
      own_frame_ = false;
    } else {
      ++counted_;
    }
  }
  if (!window_start_.has_value()) {
    window_start_ = now;
  }
  if (now - *window_start_ < window) {
    return;
  }
  window_start_ = now;
  const bool idle = counted_ == 0;
  counted_ = 0;
  if (idle && reading_.get().idle) {
    return;
  }
  own_frame_ = true;
  reading_.set(Reading{.presented = presented_, .idle = idle});
}

}
