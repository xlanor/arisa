#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace arisa::provider {

template <class Worker>
class ThreadOrphanage {
 public:
  ThreadOrphanage() = default;

  ThreadOrphanage(const ThreadOrphanage&) = delete;
  auto operator=(const ThreadOrphanage&) -> ThreadOrphanage& = delete;
  ThreadOrphanage(ThreadOrphanage&&) = delete;
  auto operator=(ThreadOrphanage&&) -> ThreadOrphanage& = delete;

  ~ThreadOrphanage() { join_all(); }

  void adopt(std::unique_ptr<Worker> worker) {
    const std::scoped_lock lock{mutex_};
    orphans_.push_back(std::move(worker));
  }

  void reap() {
    std::vector<std::unique_ptr<Worker>> finished;
    {
      const std::scoped_lock lock{mutex_};
      const auto exited = std::ranges::partition(
          orphans_, [](const std::unique_ptr<Worker>& orphan) {
            return !orphan->finished();
          });
      finished.assign(std::make_move_iterator(exited.begin()),
                      std::make_move_iterator(exited.end()));
      orphans_.erase(exited.begin(), exited.end());
    }
  }

  void join_all() noexcept {
    std::vector<std::unique_ptr<Worker>> remaining;
    {
      const std::scoped_lock lock{mutex_};
      remaining = std::exchange(orphans_, {});
    }
    for (std::unique_ptr<Worker>& orphan : remaining) {
      if (orphan->running_here()) {
        Worker::abandon(std::move(orphan));
      }
    }
  }

  auto size() const -> std::size_t {
    const std::scoped_lock lock{mutex_};
    return orphans_.size();
  }

 private:
  mutable std::mutex mutex_;
  std::vector<std::unique_ptr<Worker>> orphans_;
};

}
