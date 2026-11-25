#pragma once

#include <arisa/composition/command_options.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <string_view>

namespace arisa::components {

class CommandMapping {
 public:
  static constexpr provider::Button move = provider::Button::x;
  static constexpr provider::Button remove = provider::Button::minus;
  static constexpr provider::Button select = provider::Button::a;
  static constexpr provider::Button options = provider::Button::plus;
  static constexpr provider::Button options_hold = provider::Button::a;
  static constexpr provider::Button actions = provider::Button::y;
  static constexpr provider::Button clear = provider::Button::x;
  static constexpr provider::Button undo = provider::Button::l;
  static constexpr provider::Button redo = provider::Button::r;

  static constexpr composition::CommandOptions hold{
      .trigger = composition::CommandTrigger::long_press, .hidden = true};

  static constexpr std::string_view move_label = "Move";
  static constexpr std::string_view drop_label = "Drop";
  static constexpr std::string_view remove_label = "Delete";
  static constexpr std::string_view select_label = "Select";
  static constexpr std::string_view options_label = "Options";
  static constexpr std::string_view actions_label = "Actions";
  static constexpr std::string_view close_label = "Close";
  static constexpr std::string_view clear_label = "Clear";
  static constexpr std::string_view undo_label = "Undo";
  static constexpr std::string_view redo_label = "Redo";
};

}
