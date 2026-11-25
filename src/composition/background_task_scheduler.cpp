#include <arisa/composition/background_task_scheduler.hpp>

#include <arisa/composition/context.hpp>
#include <arisa/composition/subscription_view.hpp>

#include <utility>

namespace arisa::composition {

const EnvironmentKey<std::optional<BackgroundTaskScheduler>>
    BackgroundTaskScheduler::key{"background-task-scheduler", std::nullopt};

BackgroundTaskScheduler::BackgroundTaskScheduler()
    : state_(std::make_shared<State>()) {}

void BackgroundTaskScheduler::submit(BackgroundTask::Request request) const {
  state_->queued.push_back(std::move(request));
}

auto BackgroundTaskScheduler::pending() const noexcept -> std::size_t {
  return state_->queued.size();
}

void BackgroundTaskScheduler::run_pending() const {
  std::vector<BackgroundTask::Request> due = std::exchange(state_->queued, {});
  for (BackgroundTask::Request& request : due) {
    const std::uint64_t next = state_->deliveries.get().sequence + 1;
    state_->deliveries.set(
        Delivery{.sequence = next, .request = std::move(request)});
  }
}

auto BackgroundTaskScheduler::deliveries() const noexcept
    -> Observable<Delivery>& {
  return state_->deliveries;
}

auto BackgroundTaskScheduler::Receiver::body(Context& context) const
    -> Element {
  const std::optional<BackgroundTaskScheduler>& scheduler =
      context.environment(BackgroundTaskScheduler::key);
  Element shown = PlaceholderContentView{source_};
  if (!scheduler.has_value()) {
    return shown;
  }
  return std::move(shown).on_receive(
      scheduler->deliveries(),
      [task = task_, action = action_](const Delivery& delivery) {
        if (delivery.request.has_value() && task.matches(*delivery.request) &&
            *action) {
          (*action)(*delivery.request);
        }
      });
}

}
