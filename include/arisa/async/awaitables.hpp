#pragma once

#include <arisa/async/executor.hpp>
#include <arisa/async/frame_wait.hpp>
#include <arisa/async/job_priority.hpp>
#include <arisa/async/resume_on.hpp>
#include <arisa/async/stop_probe.hpp>

#include <chrono>
#include <stop_token>

namespace arisa::async {

inline auto on(Executor& executor,
               JobPriority priority = JobPriority::normal) noexcept
    -> ResumeOn {
  return ResumeOn{executor, priority};
}

inline auto sleep(std::chrono::nanoseconds delay) noexcept -> FrameWait {
  return FrameWait::after(delay);
}

inline auto next_frame() noexcept -> FrameWait { return FrameWait::next(); }

inline auto stop_requested() noexcept -> StopProbe<bool> { return {}; }

inline auto cancellation() noexcept -> StopProbe<std::stop_token> { return {}; }

}
