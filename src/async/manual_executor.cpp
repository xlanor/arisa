#include <arisa/async/manual_executor.hpp>

#include <algorithm>
#include <optional>
#include <utility>

namespace arisa::async {

auto ManualExecutor::schedule(Work work, JobPriority priority) -> Job {
  auto [job, ticket] = Job::create(std::move(work));
  const std::scoped_lock lock{mutex_};
  lanes_[lane(priority)].push_back(std::move(ticket));
  return std::move(job);
}

auto ManualExecutor::step() -> bool {
  std::optional<Job::Ticket> next;
  {
    const std::scoped_lock lock{mutex_};
    for (std::deque<Job::Ticket>& queue : lanes_) {
      while (!queue.empty() && !next.has_value()) {
        Job::Ticket front = std::move(queue.front());
        queue.pop_front();
        if (!front.dropped()) {
          next.emplace(std::move(front));
        }
      }
      if (next.has_value()) {
        break;
      }
    }
  }
  if (!next.has_value()) {
    return false;
  }
  next->run();
  return true;
}

auto ManualExecutor::drain() -> std::size_t {
  std::size_t ran = 0;
  while (step()) {
    ++ran;
  }
  return ran;
}

auto ManualExecutor::pending() const -> std::size_t {
  const std::scoped_lock lock{mutex_};
  std::size_t live = 0;
  for (const std::deque<Job::Ticket>& queue : lanes_) {
    live += static_cast<std::size_t>(std::ranges::count_if(
        queue, [](const Job::Ticket& ticket) { return !ticket.dropped(); }));
  }
  return live;
}

auto ManualExecutor::lane(JobPriority priority) noexcept -> std::size_t {
  return static_cast<std::size_t>(
      std::ranges::find(all_job_priorities, priority) -
      all_job_priorities.begin());
}

}
