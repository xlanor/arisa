#pragma once

#include <arisa/provider/gpu/frame_packet.hpp>

#include <chrono>

namespace arisa::provider {

class FrameStopwatch {
 public:
  explicit FrameStopwatch(FramePacket::Clock clock) noexcept : clock_(clock) {}

  auto now() const noexcept -> std::chrono::nanoseconds {
    return clock_.has_value() ? clock_->get().now().since_origin()
                              : std::chrono::nanoseconds{0};
  }

  auto timed() const noexcept -> bool { return clock_.has_value(); }

  auto since(std::chrono::nanoseconds start) const noexcept
      -> std::chrono::nanoseconds {
    return now() - start;
  }

 private:
  FramePacket::Clock clock_;
};

}
