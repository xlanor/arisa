#pragma once

#include <arisa/async/frame_scheduler.hpp>
#include <arisa/async/job.hpp>
#include <arisa/async/task_promise.hpp>

#include <chrono>
#include <concepts>
#include <coroutine>
#include <optional>
#include <utility>

namespace arisa::async {

class FrameWait {
 public:
  static auto after(std::chrono::nanoseconds delay) noexcept -> FrameWait {
    return FrameWait{delay};
  }

  static auto next() noexcept -> FrameWait { return FrameWait{std::nullopt}; }

  auto await_ready() const noexcept -> bool { return false; }

  template <class Promise>
    requires std::derived_from<Promise, TaskPromiseBase>
  auto await_suspend(std::coroutine_handle<Promise> handle) -> bool {
    const TaskControl::Scheduler scheduler =
        handle.promise().control()->scheduler();
    if (!scheduler.has_value()) {
      TaskPromiseBase::missing_scheduler();
    }
    FrameScheduler& frames = scheduler->get();
    return handle.promise().park(
        handle,
        [this, &frames](Work work) {
          return delay_.has_value() ? frames.after(*delay_, std::move(work))
                                    : frames.next_frame(std::move(work));
        },
        job_);
  }

  void await_resume() const noexcept {}

 private:
  explicit FrameWait(std::optional<std::chrono::nanoseconds> delay) noexcept
      : delay_(delay) {}

  std::optional<std::chrono::nanoseconds> delay_;
  std::optional<Job> job_;
};

}
