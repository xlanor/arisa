#pragma once

#include <arisa/provider/input/provider.hpp>
#include <arisa/runtime/frame_timings.hpp>

namespace arisa::runtime {

class FrameObserver {
 public:
  FrameObserver() = default;
  FrameObserver(const FrameObserver&) = delete;
  auto operator=(const FrameObserver&) -> FrameObserver& = delete;
  FrameObserver(FrameObserver&&) = delete;
  auto operator=(FrameObserver&&) -> FrameObserver& = delete;
  virtual ~FrameObserver() = default;

  virtual void filter(provider::InputFrame& input) = 0;

  virtual void observe(const FrameTimings& frame) = 0;
};

}
