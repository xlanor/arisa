#include <arisa/async/thread_pool.hpp>

#include <algorithm>
#include <array>
#include <condition_variable>
#include <deque>
#include <format>
#include <mutex>
#include <optional>
#include <stop_token>
#include <utility>

namespace arisa::async {

class ThreadPool::Queue {
 public:
  void push(Job::Ticket ticket, JobPriority priority) {
    {
      const std::scoped_lock lock{mutex_};
      lanes_[lane(priority)].push_back(std::move(ticket));
    }
    wake_.notify_one();
  }

  void serve(const std::stop_token& token) {
    while (true) {
      std::optional<Job::Ticket> next;
      {
        std::unique_lock lock{mutex_};
        if (!wake_.wait(lock, token,
                        [this] { return !paused_ && waiting() > 0; })) {
          return;
        }
        next.emplace(take());
        ++running_;
      }
      next->run();
      next.reset();
      {
        const std::scoped_lock lock{mutex_};
        --running_;
      }
      idle_.notify_all();
    }
  }

  void set_paused(bool paused) {
    {
      const std::scoped_lock lock{mutex_};
      paused_ = paused;
    }
    wake_.notify_all();
  }

  auto paused() const -> bool {
    const std::scoped_lock lock{mutex_};
    return paused_;
  }

  void wait_idle() const {
    std::unique_lock lock{mutex_};
    idle_.wait(lock, [this] { return waiting() == 0 && running_ == 0; });
  }

  auto pending() const -> std::size_t {
    const std::scoped_lock lock{mutex_};
    return waiting();
  }

  auto running() const -> std::size_t {
    const std::scoped_lock lock{mutex_};
    return running_;
  }

 private:
  static auto lane(JobPriority priority) noexcept -> std::size_t {
    return static_cast<std::size_t>(
        std::ranges::find(all_job_priorities, priority) -
        all_job_priorities.begin());
  }

  auto waiting() const noexcept -> std::size_t {
    std::size_t total = 0;
    for (const std::deque<Job::Ticket>& queue : lanes_) {
      total += queue.size();
    }
    return total;
  }

  auto take() -> Job::Ticket {
    for (std::deque<Job::Ticket>& queue : lanes_) {
      if (!queue.empty()) {
        Job::Ticket front = std::move(queue.front());
        queue.pop_front();
        return front;
      }
    }
    std::unreachable();
  }

  mutable std::mutex mutex_;
  std::condition_variable_any wake_;
  mutable std::condition_variable idle_;
  std::array<std::deque<Job::Ticket>, all_job_priorities.size()> lanes_;
  std::size_t running_{0};
  bool paused_{false};
};

ThreadPool::ThreadPool(provider::ThreadingProvider& threading,
                       std::shared_ptr<Queue> queue,
                       std::vector<provider::Thread> threads,
                       provider::ThreadRole role)
    : threading_(threading),
      queue_(std::move(queue)),
      pauses_(
          [held = std::weak_ptr<Queue>{queue_}]()
              -> std::expected<void, provider::ProviderError> {
            if (const std::shared_ptr<Queue> alive = held.lock()) {
              alive->set_paused(true);
            }
            return {};
          },
          [held = std::weak_ptr<Queue>{queue_}]() noexcept {
            if (const std::shared_ptr<Queue> alive = held.lock()) {
              alive->set_paused(false);
            }
          }),
      threads_(std::move(threads)),
      role_(role) {}

ThreadPool::ThreadPool(ThreadPool&& other) noexcept = default;

ThreadPool::~ThreadPool() {
  for (provider::Thread& thread : threads_) {
    thread.request_stop();
  }
}

auto ThreadPool::create(provider::ThreadingProvider& threading, Config config)
    -> std::expected<ThreadPool, provider::ProviderError> {
  auto queue = std::make_shared<Queue>();
  std::vector<provider::Thread> threads;
  threads.reserve(config.workers.value());
  for (std::uint32_t index = 0; index < config.workers.value(); ++index) {
    auto thread = threading.start(
        provider::ThreadSpec{.name = std::format("{}-{}", config.name, index),
                             .role = config.role},
        [&served = *queue](const std::stop_token& token) {
          served.serve(token);
        });
    if (!thread.has_value()) {
      return std::unexpected(thread.error());
    }
    threads.push_back(std::move(*thread));
  }
  return ThreadPool{threading, std::move(queue), std::move(threads),
                    config.role};
}

auto ThreadPool::schedule(Work work, JobPriority priority) -> Job {
  auto [job, ticket] = Job::create(std::move(work));
  queue_->push(std::move(ticket), priority);
  return std::move(job);
}

auto ThreadPool::pause() -> PauseScope {
  auto acquired = pauses_.acquire();
  return acquired.has_value() ? std::move(*acquired) : PauseScope{};
}

auto ThreadPool::paused() const -> bool { return queue_->paused(); }

auto ThreadPool::wait() const -> std::expected<void, provider::ProviderError> {
  if (on_worker()) {
    return std::unexpected(
        provider::ProviderError{provider::ProviderErrorCode::invalid_argument});
  }
  queue_->wait_idle();
  return {};
}

auto ThreadPool::pending() const -> std::size_t { return queue_->pending(); }

auto ThreadPool::running() const -> std::size_t { return queue_->running(); }

auto ThreadPool::workers() const -> std::vector<provider::ThreadId> {
  std::vector<provider::ThreadId> ids;
  ids.reserve(threads_.size());
  for (const provider::Thread& thread : threads_) {
    ids.push_back(thread.id());
  }
  return ids;
}

auto ThreadPool::on_worker() const -> bool {
  const provider::ThreadId current = threading_.get().current_thread();
  return std::ranges::any_of(threads_,
                             [current](const provider::Thread& thread) {
                               return thread.id() == current;
                             });
}

}
