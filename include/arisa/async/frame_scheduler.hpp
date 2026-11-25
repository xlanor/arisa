#pragma once

#include <arisa/async/job.hpp>

#include <chrono>

namespace arisa::async {

class FrameScheduler {
 public:
  virtual ~FrameScheduler() = default;

  virtual auto after(std::chrono::nanoseconds delay, Work work) -> Job = 0;

  virtual auto next_frame(Work work) -> Job = 0;
};

}
