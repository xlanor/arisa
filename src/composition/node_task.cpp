#include <arisa/async/resume_on.hpp>
#include <arisa/composition/node_task.hpp>

#include <type_traits>
#include <utility>

namespace arisa::composition {

NodeTask::NodeTask(std::optional<ElementKey> key, Body body) noexcept
    : key_(std::move(key)), body_(std::move(body)) {}

NodeTask::NodeTask(std::optional<ElementKey> key, Body body,
                   TaskOptions options) noexcept
    : key_(std::move(key)),
      options_(std::move(options)),
      body_(std::move(body)) {}

auto NodeTask::preferring(async::Executor& executor,
                          async::JobPriority priority, Plain make)
    -> async::Task<void> {
  co_await async::ResumeOn{executor, priority};
  co_await make();
}

auto NodeTask::preferring(async::Executor& executor,
                          async::JobPriority priority, Fallible make)
    -> async::Task<std::expected<void, UiError>> {
  co_await async::ResumeOn{executor, priority};
  co_return co_await make();
}

auto NodeTask::running() const noexcept -> bool {
  return !std::holds_alternative<std::monostate>(running_);
}

void NodeTask::start(Scheduler scheduler) {
  if (!body_.has_value()) {
    return;
  }
  Body body = std::move(*body_);
  body_.reset();
  std::visit(
      [this, scheduler](auto& make) {
        if (!make) {
          return;
        }
        using Make = std::remove_cvref_t<decltype(make)>;
        auto task = options_.executor_preference.has_value()
                        ? preferring(options_.executor_preference->get(),
                                     options_.priority, Make{std::move(make)})
                        : make();
        if (scheduler.has_value()) {
          task.start(scheduler->get());
        } else {
          task.start();
        }
        running_ = std::move(task);
      },
      body);
}

auto NodeTask::settle() -> std::optional<UiError> {
  std::optional<UiError> failure;
  const bool finished = std::visit(
      [&failure](auto& task) -> bool {
        using Held = std::remove_cvref_t<decltype(task)>;
        if constexpr (std::is_same_v<Held, std::monostate>) {
          return false;
        } else {
          if (!task.done()) {
            return false;
          }
          if constexpr (std::is_same_v<
                            Held, async::Task<std::expected<void, UiError>>>) {
            if (!task.result().has_value()) {
              failure = std::move(task.result().error());
            }
          }
          return true;
        }
      },
      running_);
  if (finished) {
    running_ = std::monostate{};
  }
  return failure;
}

}
