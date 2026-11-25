#pragma once

#include <arisa/composition/axis.hpp>
#include <arisa/composition/command_options.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace arisa::composition {

class Command {
 public:
  enum class Kind : std::uint8_t {
    move,
    exit,
    play_pause,
    button,
    edge,
    remove,
    page,
  };

  using Action = std::move_only_function<void()>;
  using MoveAction = std::move_only_function<void(MoveCommandDirection)>;

  static constexpr std::string_view exit_label = "Back";
  static constexpr std::string_view play_pause_label = "Play/Pause";
  static constexpr std::string_view remove_label = "Delete";
  static constexpr std::string_view previous_label = "Previous";
  static constexpr std::string_view next_label = "Next";

  static auto move(MoveAction action) -> Command;

  static auto move(MoveAction action, Axis axis) -> Command;

  static auto exit(std::string label, Action action) -> Command;

  static auto exit(std::string label, Action action, CommandOptions options)
      -> Command;

  static auto play_pause(std::string label, Action action) -> Command;

  static auto button(provider::Button button, std::string label, Action action,
                     CommandOptions options) -> Command;

  static auto remove(std::string label, Action action) -> Command;

  static auto page(bool forward, std::string label, Action action,
                   CommandOptions options) -> Command;

  auto forward() const noexcept -> bool {
    return edge_ == MoveCommandDirection::down;
  }

  auto kind() const noexcept -> Kind { return kind_; }

  auto pad_button() const noexcept -> std::optional<provider::Button> {
    return button_;
  }

  auto label() const noexcept -> std::string_view { return label_; }

  auto options() const noexcept -> const CommandOptions& { return options_; }

  static auto edge(MoveCommandDirection direction, Action action) -> Command;

  auto handles(MoveCommandDirection direction) const noexcept -> bool;

  auto meets(MoveCommandDirection direction) const noexcept -> bool;

  void perform();

  void perform(MoveCommandDirection direction);

 private:
  Command(Kind kind, std::optional<provider::Button> button, std::string label,
          std::variant<Action, MoveAction> action,
          CommandOptions options) noexcept;

  Kind kind_;
  std::optional<provider::Button> button_;
  std::string label_;
  std::variant<Action, MoveAction> action_;
  CommandOptions options_;
  std::optional<Axis> axis_;
  std::optional<MoveCommandDirection> edge_;
};

}
