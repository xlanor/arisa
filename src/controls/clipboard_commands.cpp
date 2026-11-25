#include <arisa/controls/clipboard_commands.hpp>

#include <arisa/components/button.hpp>
#include <arisa/composition/event_modifiers.hpp>
#include <arisa/composition/key_equivalent.hpp>
#include <arisa/composition/key_press.hpp>

#include <algorithm>
#include <string>
#include <utility>

namespace arisa::controls {

auto ClipboardCommands::can_paste() const -> bool {
  return paste_ && board_.has_value() &&
         std::ranges::any_of(accepted_,
                             [this](const composition::UTType& type) {
                               return board_->contains(type);
                             });
}

void ClipboardCommands::copy() const {
  if (copy_ && board_.has_value()) {
    board_->set_items(copy_());
  }
}

void ClipboardCommands::cut() const {
  if (cut_ && board_.has_value()) {
    board_->set_items(cut_());
  }
}

void ClipboardCommands::paste() const {
  if (!paste_ || !board_.has_value()) {
    return;
  }
  std::vector<composition::TransferItem> accepted;
  for (composition::TransferItem& item : board_->items()) {
    if (std::ranges::contains(accepted_, item.type())) {
      accepted.push_back(std::move(item));
    }
  }
  if (!accepted.empty()) {
    paste_(std::move(accepted));
  }
}

auto ClipboardCommands::items() const -> std::vector<composition::Element> {
  std::vector<composition::Element> produced;
  if (cut_) {
    produced.push_back(
        components::Button(std::string{cut_label}, [*this] { cut(); }));
  }
  if (copy_) {
    produced.push_back(
        components::Button(std::string{copy_label}, [*this] { copy(); }));
  }
  if (paste_) {
    produced.push_back(components::Button(std::string{paste_label}, [*this] {
                         paste();
                       }).disabled(!can_paste()));
  }
  return produced;
}

auto ClipboardCommands::keys(composition::Element target) const
    -> composition::Element {
  std::vector<composition::KeyEquivalent> keys;
  if (copy_) {
    keys.emplace_back('c');
  }
  if (cut_) {
    keys.emplace_back('x');
  }
  if (paste_) {
    keys.emplace_back('v');
  }
  if (keys.empty()) {
    return target;
  }
  return std::move(target).on_key_press(
      std::move(keys), composition::KeyPress::Phases::down,
      [commands = *this](const composition::KeyPress& press) {
        const bool chorded =
            press.modifiers.contains(composition::EventModifiers::command) ||
            press.modifiers.contains(composition::EventModifiers::control);
        if (!chorded) {
          return composition::KeyPress::Result::ignored;
        }
        if (press.key == composition::KeyEquivalent{'c'}) {
          commands.copy();
        } else if (press.key == composition::KeyEquivalent{'x'}) {
          commands.cut();
        } else {
          commands.paste();
        }
        return composition::KeyPress::Result::handled;
      });
}

}
