#pragma once

#include <arisa/async/frame_scheduler.hpp>
#include <arisa/async/resume_on.hpp>
#include <arisa/async/task.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/task_options.hpp>
#include <arisa/composition/ui_error.hpp>

#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <variant>

namespace arisa::composition {

class NodeTask {
 public:
  using Plain = std::move_only_function<async::Task<void>()>;
  using Fallible =
      std::move_only_function<async::Task<std::expected<void, UiError>>()>;
  using Body = std::variant<Plain, Fallible>;
  using Scheduler =
      std::optional<std::reference_wrapper<async::FrameScheduler>>;

  NodeTask(std::optional<ElementKey> key, Body body) noexcept;

  NodeTask(std::optional<ElementKey> key, Body body,
           TaskOptions options) noexcept;

  auto key() const noexcept -> const std::optional<ElementKey>& { return key_; }

  auto name() const noexcept -> const std::optional<std::string>& {
    return options_.name;
  }

  auto priority() const noexcept -> async::JobPriority {
    return options_.priority;
  }

  auto options() const noexcept -> const TaskOptions& { return options_; }

  auto pending() const noexcept -> bool { return body_.has_value(); }

  auto running() const noexcept -> bool;

  void start(Scheduler scheduler);

  auto settle() -> std::optional<UiError>;

 private:
  using Running = std::variant<std::monostate, async::Task<void>,
                               async::Task<std::expected<void, UiError>>>;

  static auto preferring(async::Executor& executor, async::JobPriority priority,
                         Plain make) -> async::Task<void>;

  static auto preferring(async::Executor& executor, async::JobPriority priority,
                         Fallible make)
      -> async::Task<std::expected<void, UiError>>;

  std::optional<ElementKey> key_;
  TaskOptions options_;
  std::optional<Body> body_;
  Running running_;
};

}
