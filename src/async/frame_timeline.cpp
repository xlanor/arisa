#include <arisa/async/frame_timeline.hpp>

#include <algorithm>
#include <iterator>
#include <utility>

namespace arisa::async {

FrameTimeline::FrameTimeline(Clock clock) noexcept : clock_(std::move(clock)) {}

auto FrameTimeline::after(std::chrono::nanoseconds delay, Work work) -> Job {
  auto [job, ticket] = Job::create(std::move(work));
  const std::chrono::nanoseconds deadline = clock_() + delay;
  const std::scoped_lock lock{mutex_};
  if (!closed_) {
    timers_.push_back(Timer{.deadline = deadline, .ticket = std::move(ticket)});
  }
  return std::move(job);
}

auto FrameTimeline::next_frame(Work work) -> Job {
  auto [job, ticket] = Job::create(std::move(work));
  const std::scoped_lock lock{mutex_};
  if (!closed_) {
    waiters_.push_back(std::move(ticket));
  }
  return std::move(job);
}

auto FrameTimeline::fire() -> std::size_t {
  const std::chrono::nanoseconds now = clock_();
  std::vector<Timer> due;
  std::vector<Job::Ticket> waiters;
  {
    const std::scoped_lock lock{mutex_};
    std::erase_if(timers_,
                  [](const Timer& timer) { return timer.ticket.dropped(); });
    const auto ready = std::ranges::partition(
        timers_, [now](const Timer& timer) { return timer.deadline > now; });
    due.assign(std::make_move_iterator(ready.begin()),
               std::make_move_iterator(ready.end()));
    timers_.erase(ready.begin(), ready.end());
    waiters = std::exchange(waiters_, {});
  }
  std::ranges::stable_sort(due, {}, &Timer::deadline);
  std::size_t ran = 0;
  for (Timer& timer : due) {
    if (!timer.ticket.dropped()) {
      timer.ticket.run();
      ++ran;
    }
  }
  for (Job::Ticket& waiter : waiters) {
    if (!waiter.dropped()) {
      waiter.run();
      ++ran;
    }
  }
  return ran;
}

void FrameTimeline::close() noexcept {
  std::vector<Timer> timers;
  std::vector<Job::Ticket> waiters;
  {
    const std::scoped_lock lock{mutex_};
    closed_ = true;
    timers = std::exchange(timers_, {});
    waiters = std::exchange(waiters_, {});
  }
}

auto FrameTimeline::pending() const -> std::size_t {
  const std::scoped_lock lock{mutex_};
  const auto live_timers = std::ranges::count_if(
      timers_, [](const Timer& timer) { return !timer.ticket.dropped(); });
  const auto live_waiters = std::ranges::count_if(
      waiters_, [](const Job::Ticket& ticket) { return !ticket.dropped(); });
  return static_cast<std::size_t>(live_timers + live_waiters);
}

auto FrameTimeline::next_deadline() const
    -> std::optional<std::chrono::nanoseconds> {
  const std::scoped_lock lock{mutex_};
  std::optional<std::chrono::nanoseconds> earliest;
  for (const Timer& timer : timers_) {
    if (!timer.ticket.dropped() &&
        (!earliest.has_value() || timer.deadline < *earliest)) {
      earliest = timer.deadline;
    }
  }
  return earliest;
}

auto FrameTimeline::awaiting_frame() const -> bool {
  const std::scoped_lock lock{mutex_};
  return std::ranges::any_of(
      waiters_, [](const Job::Ticket& ticket) { return !ticket.dropped(); });
}

}
