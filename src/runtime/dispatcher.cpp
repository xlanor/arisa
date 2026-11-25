#include <arisa/runtime/dispatcher.hpp>

#include <runtime/dispatch_queue.hpp>

#include <utility>

namespace arisa::runtime {

Dispatcher::Dispatcher(std::shared_ptr<DispatchQueue> queue) noexcept
    : queue_(std::move(queue)) {}

auto Dispatcher::post(Task task) const -> bool {
  return queue_->push(std::move(task));
}

auto Dispatcher::schedule(async::Work work, async::JobPriority) -> async::Job {
  auto [job, ticket] = async::Job::create(std::move(work));
  static_cast<void>(
      post([ticket = std::move(ticket)]() mutable { ticket.run(); }));
  return std::move(job);
}

}
