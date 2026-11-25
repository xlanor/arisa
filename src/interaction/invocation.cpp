#include <interaction/invocation.hpp>

#include <utility>

namespace arisa::interaction {

auto Invocation::activation(composition::NodeId node) -> Invocation {
  return Invocation{node, nullptr, std::nullopt};
}

auto Invocation::activation(composition::NodeId node, composition::Point at)
    -> Invocation {
  Invocation invocation{node, nullptr, std::nullopt};
  invocation.at_ = at;
  return invocation;
}

auto Invocation::command(std::shared_ptr<composition::Command> command)
    -> Invocation {
  return Invocation{std::nullopt, std::move(command), std::nullopt};
}

auto Invocation::move(std::shared_ptr<composition::Command> command,
                      composition::MoveCommandDirection direction)
    -> Invocation {
  return Invocation{std::nullopt, std::move(command), direction};
}

auto Invocation::callback(std::move_only_function<void()> action)
    -> Invocation {
  Invocation invocation{std::nullopt, nullptr, std::nullopt};
  invocation.action_ = std::move(action);
  return invocation;
}

void Invocation::perform(composition::Tree& tree) {
  if (action_) {
    action_();
    return;
  }
  if (node_.has_value()) {
    static_cast<void>(tree.activate(*node_, at_));
    return;
  }
  if (command_ == nullptr) {
    return;
  }
  if (direction_.has_value()) {
    command_->perform(*direction_);
    return;
  }
  command_->perform();
}

}
