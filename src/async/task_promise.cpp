#include <arisa/async/frame_pool.hpp>
#include <arisa/async/task_promise.hpp>

#include <cstdio>
#include <exception>
#include <print>

namespace arisa::async {

auto TaskPromiseBase::FinalAwaiter::await_suspend(
    std::coroutine_handle<>) const noexcept -> std::coroutine_handle<> {
  const TaskPromiseBase& promise = promise_;
  if (promise.continuation_) {
    return promise.continuation_;
  }
  const std::shared_ptr<TaskControl> control = promise.control_;
  control->complete();
  return std::noop_coroutine();
}

auto TaskPromiseBase::operator new(std::size_t size) -> void* {
  return FramePool::shared().allocate(size);
}

void TaskPromiseBase::operator delete(void* frame, std::size_t size) noexcept {
  FramePool::shared().deallocate(frame, size);
}

void TaskPromiseBase::unhandled_exception() noexcept { std::terminate(); }

void TaskPromiseBase::missing_scheduler() noexcept {
  std::println(stderr,
               "arisa: a task awaited sleep() or next_frame() without a frame "
               "scheduler");
  std::terminate();
}

void TaskPromiseBase::launch(std::coroutine_handle<> self,
                             TaskControl::Scheduler scheduler) {
  control_ = std::make_shared<TaskControl>(self, scheduler);
  const std::shared_ptr<TaskControl> control = control_;
  if (control->begin()) {
    self.resume();
  }
}

void TaskPromiseBase::adopt(const TaskPromiseBase& parent,
                            std::coroutine_handle<> continuation) noexcept {
  control_ = parent.control_;
  continuation_ = continuation;
}

}
