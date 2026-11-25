#pragma once

#include <arisa/async/frame_scheduler.hpp>
#include <arisa/async/job.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>

namespace arisa::async {

class FrameTimeline final : public FrameScheduler {
 public:
  using Clock = std::move_only_function<std::chrono::nanoseconds() const>;

  explicit FrameTimeline(Clock clock) noexcept;

  auto after(std::chrono::nanoseconds delay, Work work) -> Job override;

  auto next_frame(Work work) -> Job override;

  auto fire() -> std::size_t;

  void close() noexcept;

  auto pending() const -> std::size_t;

  auto next_deadline() const -> std::optional<std::chrono::nanoseconds>;

  auto awaiting_frame() const -> bool;

  auto now() const -> std::chrono::nanoseconds { return clock_(); }

 private:
  struct Timer {
    std::chrono::nanoseconds deadline;
    Job::Ticket ticket;
  };

  Clock clock_;
  mutable std::mutex mutex_;
  std::vector<Timer> timers_;
  std::vector<Job::Ticket> waiters_;
  bool closed_{false};
};

}
