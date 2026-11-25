#include <runtime/dispatch_queue.hpp>

#include <utility>

namespace arisa::runtime {

auto DispatchQueue::push(Dispatcher::Task task) -> bool {
  {
    const std::scoped_lock lock{mutex_};
    if (closed_) {
      return false;
    }
    pending_.push_back(std::move(task));
  }
  arrived_.notify_all();
  return true;
}

auto DispatchQueue::drain() -> std::size_t {
  std::vector<Dispatcher::Task> ready;
  {
    const std::scoped_lock lock{mutex_};
    ready = std::exchange(pending_, {});
  }
  for (Dispatcher::Task& task : ready) {
    task();
  }
  return ready.size();
}

void DispatchQueue::close() noexcept {
  std::vector<Dispatcher::Task> dropped;
  {
    const std::scoped_lock lock{mutex_};
    closed_ = true;
    dropped = std::exchange(pending_, {});
  }
  arrived_.notify_all();
}

auto DispatchQueue::wait_for(std::chrono::nanoseconds limit) -> bool {
  std::unique_lock lock{mutex_};
  return arrived_.wait_for(lock, limit,
                           [this] { return closed_ || !pending_.empty(); });
}

auto DispatchQueue::pending() -> bool {
  const std::scoped_lock lock{mutex_};
  return !pending_.empty();
}

}
