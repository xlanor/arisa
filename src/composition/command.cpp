#include <arisa/composition/command.hpp>

#include <utility>

namespace arisa::composition {

Command::Command(Kind kind, std::optional<provider::Button> button,
                 std::string label, std::variant<Action, MoveAction> action,
                 CommandOptions options) noexcept
    : kind_(kind),
      button_(button),
      label_(std::move(label)),
      action_(std::move(action)),
      options_(options) {}

auto Command::move(MoveAction action) -> Command {
  return Command{
      Kind::move, std::nullopt, std::string{},
      std::variant<Action, MoveAction>{std::in_place_index<1>,
                                       std::move(action)},
      CommandOptions{.trigger = CommandTrigger::repeat, .hidden = true}};
}

auto Command::move(MoveAction action, Axis axis) -> Command {
  Command command = move(std::move(action));
  command.axis_ = axis;
  return command;
}

auto Command::edge(MoveCommandDirection direction, Action action) -> Command {
  Command command{
      Kind::edge, std::nullopt, std::string{},
      std::variant<Action, MoveAction>{std::in_place_index<0>,
                                       std::move(action)},
      CommandOptions{.trigger = CommandTrigger::press, .hidden = true}};
  command.edge_ = direction;
  return command;
}

auto Command::meets(MoveCommandDirection direction) const noexcept -> bool {
  return kind_ == Kind::edge && edge_ == direction;
}

auto Command::handles(MoveCommandDirection direction) const noexcept -> bool {
  if (kind_ != Kind::move) {
    return false;
  }
  if (!axis_.has_value()) {
    return true;
  }
  const bool sideways = direction == MoveCommandDirection::left ||
                        direction == MoveCommandDirection::right;
  return sideways == (*axis_ == Axis::horizontal);
}

auto Command::exit(std::string label, Action action) -> Command {
  return exit(std::move(label), std::move(action), CommandOptions{});
}

auto Command::exit(std::string label, Action action, CommandOptions options)
    -> Command {
  return Command{Kind::exit, std::nullopt, std::move(label),
                 std::variant<Action, MoveAction>{std::in_place_index<0>,
                                                  std::move(action)},
                 options};
}

auto Command::play_pause(std::string label, Action action) -> Command {
  return Command{Kind::play_pause, std::nullopt, std::move(label),
                 std::variant<Action, MoveAction>{std::in_place_index<0>,
                                                  std::move(action)},
                 CommandOptions{}};
}

auto Command::button(provider::Button button, std::string label, Action action,
                     CommandOptions options) -> Command {
  return Command{Kind::button, button, std::move(label),
                 std::variant<Action, MoveAction>{std::in_place_index<0>,
                                                  std::move(action)},
                 options};
}

auto Command::remove(std::string label, Action action) -> Command {
  return Command{Kind::remove, std::nullopt, std::move(label),
                 std::variant<Action, MoveAction>{std::in_place_index<0>,
                                                  std::move(action)},
                 CommandOptions{}};
}

auto Command::page(bool forward, std::string label, Action action,
                   CommandOptions options) -> Command {
  Command command{Kind::page, std::nullopt, std::move(label),
                  std::variant<Action, MoveAction>{std::in_place_index<0>,
                                                   std::move(action)},
                  options};
  command.edge_ =
      forward ? MoveCommandDirection::down : MoveCommandDirection::up;
  return command;
}

void Command::perform() {
  if (Action* action = std::get_if<Action>(&action_);
      action != nullptr && *action) {
    (*action)();
  }
}

void Command::perform(MoveCommandDirection direction) {
  if (MoveAction* action = std::get_if<MoveAction>(&action_);
      action != nullptr && *action) {
    (*action)(direction);
    return;
  }
  perform();
}

}
