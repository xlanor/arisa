#include <arisa/async/task_control.hpp>

#include <utility>

namespace arisa::async {

TaskControl::TaskControl(std::coroutine_handle<> root,
                         Scheduler scheduler) noexcept
    : root_(root), scheduler_(scheduler) {}

auto TaskControl::begin() noexcept -> bool {
  Phase expected = Phase::idle;
  return phase_.compare_exchange_strong(expected, Phase::running,
                                        std::memory_order_acq_rel);
}

auto TaskControl::settle() noexcept -> bool {
  Phase expected = Phase::running;
  if (phase_.compare_exchange_strong(expected, Phase::idle,
                                     std::memory_order_acq_rel)) {
    phase_.notify_all();
    return true;
  }
  if (expected == Phase::abandoned) {
    destroy_root();
  }
  return true;
}

void TaskControl::wake(std::coroutine_handle<> suspended) noexcept {
  Phase current = phase_.load(std::memory_order_acquire);
  while (true) {
    switch (current) {
      case Phase::idle:
        if (phase_.compare_exchange_weak(current, Phase::running,
                                         std::memory_order_acq_rel)) {
          suspended.resume();
          return;
        }
        break;
      case Phase::running:
        phase_.wait(Phase::running, std::memory_order_acquire);
        current = phase_.load(std::memory_order_acquire);
        break;
      case Phase::completed:
      case Phase::abandoned:
      case Phase::destroyed:
        return;
    }
  }
}

void TaskControl::complete() noexcept {
  Phase expected = Phase::running;
  if (phase_.compare_exchange_strong(expected, Phase::completed,
                                     std::memory_order_acq_rel)) {
    phase_.notify_all();
    return;
  }
  if (expected == Phase::abandoned) {
    destroy_root();
  }
}

void TaskControl::release() noexcept {
  stop_.request_stop();
  Phase current = phase_.load(std::memory_order_acquire);
  while (true) {
    switch (current) {
      case Phase::idle:
      case Phase::completed:
        if (phase_.compare_exchange_weak(current, Phase::destroyed,
                                         std::memory_order_acq_rel)) {
          phase_.notify_all();
          std::exchange(root_, nullptr).destroy();
          return;
        }
        break;
      case Phase::running:
        if (phase_.compare_exchange_weak(current, Phase::abandoned,
                                         std::memory_order_acq_rel)) {
          phase_.notify_all();
          return;
        }
        break;
      case Phase::abandoned:
      case Phase::destroyed:
        return;
    }
  }
}

void TaskControl::destroy_root() noexcept {
  phase_.store(Phase::destroyed, std::memory_order_release);
  phase_.notify_all();
  std::exchange(root_, nullptr).destroy();
}

}
