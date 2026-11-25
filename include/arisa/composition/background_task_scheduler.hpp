#pragma once

#include <arisa/composition/background_task.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/placeholder_content_view.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::composition {

class Context;

class BackgroundTaskScheduler {
 public:
  class Receiver;

  struct Delivery {
    std::uint64_t sequence{0};
    std::optional<BackgroundTask::Request> request{};

    friend auto operator==(const Delivery& left, const Delivery& right)
        -> bool {
      return left.sequence == right.sequence;
    }
  };

  static const EnvironmentKey<std::optional<BackgroundTaskScheduler>> key;

  BackgroundTaskScheduler();

  void submit(BackgroundTask::Request request) const;

  auto pending() const noexcept -> std::size_t;

  void run_pending() const;

  auto deliveries() const noexcept -> Observable<Delivery>&;

  friend auto operator==(const BackgroundTaskScheduler& left,
                         const BackgroundTaskScheduler& right) -> bool {
    return left.state_ == right.state_;
  }

 private:
  struct State {
    Observable<Delivery> deliveries;
    std::vector<BackgroundTask::Request> queued;
  };

  std::shared_ptr<State> state_;
};

class BackgroundTaskScheduler::Receiver final : public Component {
 public:
  Receiver(Element content, BackgroundTask task, BackgroundTask::Action action)
      : source_(std::make_shared<PlaceholderContentView::Source>(
            std::move(content))),
        task_(std::move(task)),
        action_(
            std::make_shared<const BackgroundTask::Action>(std::move(action))) {
  }

  auto operator==(const Receiver& other) const -> bool {
    return source_ == other.source_ && task_ == other.task_ &&
           action_ == other.action_;
  }

  auto body(Context& context) const -> Element override;

 private:
  std::shared_ptr<PlaceholderContentView::Source> source_;
  BackgroundTask task_;
  std::shared_ptr<const BackgroundTask::Action> action_;
};

template <class Action>
auto Element::background_task(BackgroundTask task,
                              Action action) && -> Element {
  return BackgroundTaskScheduler::Receiver{
      std::move(*this), std::move(task),
      BackgroundTask::Action{std::move(action)}};
}

}
