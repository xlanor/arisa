#pragma once

#include <arisa/async/task_promise.hpp>

#include <concepts>
#include <coroutine>
#include <stop_token>
#include <type_traits>

namespace arisa::async {

template <class Result>
  requires std::same_as<Result, bool> || std::same_as<Result, std::stop_token>
class StopProbe {
 public:
  auto await_ready() const noexcept -> bool { return false; }

  template <class Promise>
    requires std::derived_from<Promise, TaskPromiseBase>
  auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> bool {
    token_ = handle.promise().control()->stop_token();
    return false;
  }

  auto await_resume() const noexcept -> Result {
    if constexpr (std::is_same_v<Result, bool>) {
      return token_.stop_requested();
    } else {
      return token_;
    }
  }

 private:
  std::stop_token token_;
};

}
