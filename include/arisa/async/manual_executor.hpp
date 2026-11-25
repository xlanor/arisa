#pragma once

#include <arisa/async/executor.hpp>
#include <arisa/async/job_priority.hpp>

#include <array>
#include <cstddef>
#include <deque>
#include <mutex>

namespace arisa::async {

class ManualExecutor final : public Executor {
 public:
  auto step() -> bool;

  auto drain() -> std::size_t;

  auto pending() const -> std::size_t;

 private:
  auto schedule(Work work, JobPriority priority) -> Job override;

  static auto lane(JobPriority priority) noexcept -> std::size_t;

  mutable std::mutex mutex_;
  std::array<std::deque<Job::Ticket>, all_job_priorities.size()> lanes_;
};

}
