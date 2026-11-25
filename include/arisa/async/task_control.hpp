#pragma once

#include <arisa/async/frame_scheduler.hpp>

#include <atomic>
#include <coroutine>
#include <cstdint>
#include <functional>
#include <optional>
#include <stop_token>

namespace arisa::async {

class TaskControl {
 public:
  using Scheduler = std::optional<std::reference_wrapper<FrameScheduler>>;

  TaskControl(std::coroutine_handle<> root, Scheduler scheduler) noexcept;

  auto begin() noexcept -> bool;

  auto settle() noexcept -> bool;

  void wake(std::coroutine_handle<> suspended) noexcept;

  void complete() noexcept;

  void release() noexcept;

  auto owns(std::coroutine_handle<> frame) const noexcept -> bool {
    return frame == root_;
  }

  auto completed() const noexcept -> bool {
    return phase_.load(std::memory_order_acquire) == Phase::completed;
  }

  auto stop_token() const noexcept -> std::stop_token {
    return stop_.get_token();
  }

  auto scheduler() const noexcept -> Scheduler { return scheduler_; }

 private:
  enum class Phase : std::uint8_t {
    idle,
    running,
    completed,
    abandoned,
    destroyed,
  };

  void destroy_root() noexcept;

  std::atomic<Phase> phase_{Phase::idle};
  std::stop_source stop_;
  std::coroutine_handle<> root_;
  Scheduler scheduler_;
};

}
