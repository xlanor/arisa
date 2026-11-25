#pragma once

#include <arisa/async/executor.hpp>
#include <arisa/async/job.hpp>
#include <arisa/async/job_priority.hpp>
#include <arisa/async/task_promise.hpp>

#include <concepts>
#include <coroutine>
#include <functional>
#include <optional>
#include <utility>

namespace arisa::async {

class ResumeOn {
 public:
  explicit ResumeOn(Executor& executor,
                    JobPriority priority = JobPriority::normal) noexcept
      : executor_(executor), priority_(priority) {}

  auto await_ready() const noexcept -> bool { return false; }

  template <class Promise>
    requires std::derived_from<Promise, TaskPromiseBase>
  auto await_suspend(std::coroutine_handle<Promise> handle) -> bool {
    return handle.promise().park(
        handle,
        [this](Work work) {
          return executor_.get().submit(std::move(work), priority_);
        },
        job_);
  }

  void await_resume() const noexcept {}

 private:
  std::reference_wrapper<Executor> executor_;
  JobPriority priority_;
  std::optional<Job> job_;
};

}
