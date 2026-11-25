#pragma once

#include <arisa/async/job.hpp>
#include <arisa/async/task_control.hpp>

#include <concepts>
#include <coroutine>
#include <cstddef>
#include <memory>
#include <optional>
#include <stop_token>
#include <utility>

namespace arisa::async {

class TaskPromiseBase {
 public:
  class FinalAwaiter {
   public:
    explicit FinalAwaiter(TaskPromiseBase& promise) noexcept
        : promise_(promise) {}

    auto await_ready() const noexcept -> bool { return false; }

    auto await_suspend(std::coroutine_handle<> self) const noexcept
        -> std::coroutine_handle<>;

    void await_resume() const noexcept {}

   private:
    std::reference_wrapper<TaskPromiseBase> promise_;
  };

  static auto operator new(std::size_t size) -> void*;

  static void operator delete(void* frame, std::size_t size) noexcept;

  auto initial_suspend() const noexcept -> std::suspend_always { return {}; }

  auto final_suspend() noexcept -> FinalAwaiter { return FinalAwaiter{*this}; }

  [[noreturn]] static void unhandled_exception() noexcept;

  [[noreturn]] static void missing_scheduler() noexcept;

  auto control() const noexcept -> const std::shared_ptr<TaskControl>& {
    return control_;
  }

  template <class Schedule>
    requires std::invocable<Schedule, Work>
  auto park(std::coroutine_handle<> self, Schedule&& schedule,
            std::optional<Job>& job) -> bool {
    const std::shared_ptr<TaskControl> control = control_;
    job.emplace(std::forward<Schedule>(schedule)(
        [control, self](const std::stop_token&) { control->wake(self); }));
    return control->settle();
  }

  void launch(std::coroutine_handle<> self, TaskControl::Scheduler scheduler);

  void adopt(const TaskPromiseBase& parent,
             std::coroutine_handle<> continuation) noexcept;

  auto continuation() const noexcept -> std::coroutine_handle<> {
    return continuation_;
  }

 private:
  std::shared_ptr<TaskControl> control_;
  std::coroutine_handle<> continuation_;
};

}
