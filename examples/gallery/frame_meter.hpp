#pragma once

#include <arisa/composition/observable.hpp>
#include <arisa/runtime/frame_timings.hpp>

#include <chrono>
#include <cstdint>
#include <optional>

namespace arisa::examples {

class FrameMeter {
 public:
  struct Reading {
    std::uint64_t presented{0};
    bool idle{false};

    friend auto operator==(const Reading&, const Reading&) -> bool = default;
  };

  static constexpr std::chrono::seconds window{1};

  void record(const runtime::FrameTimings& timings,
              std::chrono::nanoseconds now);

  auto reading() const noexcept -> const composition::Observable<Reading>& {
    return reading_;
  }

 private:
  composition::Observable<Reading> reading_;
  std::optional<std::chrono::nanoseconds> window_start_;
  std::uint64_t presented_{0};
  std::uint64_t counted_{0};
  bool own_frame_{false};
};

}
