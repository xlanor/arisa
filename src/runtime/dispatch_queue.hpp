#pragma once

#include <arisa/runtime/dispatcher.hpp>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <vector>

namespace arisa::runtime {

class DispatchQueue {
 public:
  auto push(Dispatcher::Task task) -> bool;

  auto drain() -> std::size_t;

  void close() noexcept;

  auto wait_for(std::chrono::nanoseconds limit) -> bool;

  auto pending() -> bool;

 private:
  std::mutex mutex_;
  std::condition_variable arrived_;
  std::vector<Dispatcher::Task> pending_;
  bool closed_{false};
};

}
