#pragma once

#include <arisa/async/executor.hpp>
#include <arisa/async/job_priority.hpp>
#include <arisa/provider/counted_scope.hpp>
#include <arisa/provider/error.hpp>
#include <arisa/provider/threading/provider.hpp>
#include <arisa/support/bounded_value.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace arisa::async {

struct WorkerCountTag;

using WorkerCount = support::BoundedValue<std::uint32_t, 1, 64, WorkerCountTag>;

class ThreadPool final : public Executor {
 public:
  using PauseScope = support::ScopedCallback;

  struct Config {
    WorkerCount workers{WorkerCount::of<1>()};
    provider::ThreadRole role{provider::ThreadRole::background};
    std::string name{"arisa-pool"};
  };

  [[nodiscard]] static auto create(provider::ThreadingProvider& threading,
                                   Config config)
      -> std::expected<ThreadPool, provider::ProviderError>;

  ThreadPool(const ThreadPool&) = delete;
  auto operator=(const ThreadPool&) -> ThreadPool& = delete;
  ThreadPool(ThreadPool&& other) noexcept;
  auto operator=(ThreadPool&&) -> ThreadPool& = delete;
  ~ThreadPool() override;

  auto pause() -> PauseScope;

  auto paused() const -> bool;

  [[nodiscard]] auto wait() const
      -> std::expected<void, provider::ProviderError>;

  auto pending() const -> std::size_t;

  auto running() const -> std::size_t;

  auto workers() const -> std::vector<provider::ThreadId>;

  auto role() const noexcept -> provider::ThreadRole { return role_; }

 private:
  class Queue;

  ThreadPool(provider::ThreadingProvider& threading,
             std::shared_ptr<Queue> queue,
             std::vector<provider::Thread> threads, provider::ThreadRole role);

  auto schedule(Work work, JobPriority priority) -> Job override;

  auto on_worker() const -> bool;

  std::reference_wrapper<provider::ThreadingProvider> threading_;
  std::shared_ptr<Queue> queue_;
  provider::CountedScope pauses_;
  std::vector<provider::Thread> threads_;
  provider::ThreadRole role_;
};

}
