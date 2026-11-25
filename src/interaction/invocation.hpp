#pragma once

#include <arisa/composition/command.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/tree.hpp>

#include <functional>
#include <memory>
#include <optional>

namespace arisa::interaction {

class Invocation {
 public:
  static auto activation(composition::NodeId node) -> Invocation;

  static auto activation(composition::NodeId node, composition::Point at)
      -> Invocation;

  static auto command(std::shared_ptr<composition::Command> command)
      -> Invocation;

  static auto move(std::shared_ptr<composition::Command> command,
                   composition::MoveCommandDirection direction) -> Invocation;

  static auto callback(std::move_only_function<void()> action) -> Invocation;

  void perform(composition::Tree& tree);

  auto activated() const noexcept -> std::optional<composition::NodeId> {
    return node_;
  }

 private:
  Invocation(
      std::optional<composition::NodeId> node,
      std::shared_ptr<composition::Command> command,
      std::optional<composition::MoveCommandDirection> direction) noexcept
      : node_(node), command_(std::move(command)), direction_(direction) {}

  std::optional<composition::NodeId> node_;
  std::shared_ptr<composition::Command> command_;
  std::optional<composition::MoveCommandDirection> direction_;
  std::optional<composition::Point> at_;
  std::move_only_function<void()> action_;
};

}
