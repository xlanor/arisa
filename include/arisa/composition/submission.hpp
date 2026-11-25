#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/submit_triggers.hpp>
#include <arisa/provider/text_input/text_input_traits.hpp>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace arisa::composition {

class Context;

using SubmitLabel = provider::SubmitLabel;

class Submission {
 public:
  using Action = std::shared_ptr<std::move_only_function<void()>>;

  class Handlers {
   public:
    void add(std::move_only_function<void()> action);

    void scope() noexcept { scoped_ = true; }

    auto inside() const noexcept -> const std::vector<Action>& {
      return inside_;
    }

    auto outside() const noexcept -> const std::vector<Action>& {
      return outside_;
    }

    auto scoped() const noexcept -> bool { return scoped_; }

    friend auto operator==(const Handlers&, const Handlers&) -> bool = default;

   private:
    std::vector<Action> inside_;
    std::vector<Action> outside_;
    bool scoped_{false};
  };

  static const EnvironmentKey<Handlers> handlers;
  static const EnvironmentKey<Handlers> search_handlers;
  static const EnvironmentKey<SubmitLabel> label;

  static auto collect(Context& context) -> Submission;

  static auto collect(Context& context, SubmitTriggers triggers) -> Submission;

  void operator()() const;

  auto count() const noexcept -> std::size_t { return actions_.size(); }

 private:
  explicit Submission(std::vector<Action> actions) noexcept
      : actions_(std::move(actions)) {}

  static void gather(Context& context, const EnvironmentKey<Handlers>& key,
                     std::vector<Action>& actions);

  std::vector<Action> actions_;
};

}
