#pragma once

#include <arisa/async/frame_scheduler.hpp>
#include <arisa/async/task_control.hpp>
#include <arisa/async/task_promise.hpp>
#include <arisa/async/task_value.hpp>

#include <concepts>
#include <coroutine>
#include <functional>
#include <memory>
#include <stop_token>
#include <type_traits>
#include <utility>

namespace arisa::async {

template <class T = void>
class [[nodiscard]] Task {
 public:
  class promise_type : public TaskPromiseBase, public TaskValue<T> {
   public:
    auto get_return_object() noexcept -> Task {
      return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
  };

  class Awaiter {
   public:
    explicit Awaiter(std::coroutine_handle<promise_type> child) noexcept
        : child_(child) {}

    auto await_ready() const noexcept -> bool { return false; }

    template <class Promise>
      requires std::derived_from<Promise, TaskPromiseBase>
    auto await_suspend(std::coroutine_handle<Promise> parent) const noexcept
        -> std::coroutine_handle<> {
      child_.promise().adopt(parent.promise(), parent);
      return child_;
    }

    auto await_resume() const -> T { return child_.promise().take(); }

   private:
    std::coroutine_handle<promise_type> child_;
  };

  Task(const Task&) = delete;
  auto operator=(const Task&) -> Task& = delete;

  Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}

  auto operator=(Task&& other) noexcept -> Task& {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, {});
    }
    return *this;
  }

  ~Task() { reset(); }

  void start() { handle_.promise().launch(handle_, std::nullopt); }

  void start(FrameScheduler& scheduler) {
    handle_.promise().launch(handle_, std::ref(scheduler));
  }

  auto started() const noexcept -> bool {
    return handle_ && handle_.promise().control() != nullptr;
  }

  auto done() const noexcept -> bool {
    return started() && handle_.promise().control()->completed();
  }

  auto result() -> std::add_lvalue_reference_t<T>
    requires(!std::is_void_v<T>)
  {
    return handle_.promise().value();
  }

  auto stop_token() const noexcept -> std::stop_token {
    return started() ? handle_.promise().control()->stop_token()
                     : std::stop_token{};
  }

  auto operator co_await() const noexcept -> Awaiter {
    return Awaiter{handle_};
  }

 private:
  explicit Task(std::coroutine_handle<promise_type> handle) noexcept
      : handle_(handle) {}

  void reset() noexcept {
    if (!handle_) {
      return;
    }
    const auto handle = std::exchange(handle_, {});
    const std::shared_ptr<TaskControl> control = handle.promise().control();
    if (control != nullptr && control->owns(handle)) {
      control->release();
      return;
    }
    handle.destroy();
  }

  std::coroutine_handle<promise_type> handle_;
};

}
