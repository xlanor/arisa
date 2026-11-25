#include <arisa/controls/text_editing.hpp>

#include <arisa/composition/event_modifiers.hpp>
#include <arisa/composition/key_equivalent.hpp>
#include <arisa/text/utf8.hpp>

#include <algorithm>
#include <utility>

namespace arisa::controls {

TextEditing::TextEditing(std::string text, composition::TextSelection selection,
                         bool multiline)
    : text_(std::move(text)),
      selection_(std::move(selection)),
      multiline_(multiline) {
  const Range clamped = selection_.range().clamped(text_.size());
  const Range snapped_range{
      .lower = text::Utf8::boundary(text_, clamped.lower),
      .upper = text::Utf8::boundary(text_, clamped.upper)};
  if (snapped_range != selection_.range() ||
      selection_.indices().is_multi_selection()) {
    selection_ = composition::TextSelection{snapped_range}.with_affinity(
        selection_.affinity());
  }
  anchor_ =
      selection_.affinity() == composition::TextSelectionAffinity::upstream
          ? snapped_range.upper
          : snapped_range.lower;
}

auto TextEditing::word_character(char32_t codepoint) noexcept -> bool {
  if (codepoint >= 0x80) {
    return codepoint != 0x00A0 && codepoint != 0x3000;
  }
  return (codepoint >= U'a' && codepoint <= U'z') ||
         (codepoint >= U'A' && codepoint <= U'Z') ||
         (codepoint >= U'0' && codepoint <= U'9') || codepoint == U'_' ||
         codepoint == U'\'';
}

auto TextEditing::word_end(std::string_view text, std::size_t at) noexcept
    -> std::size_t {
  while (at < text.size() &&
         !word_character(text::Utf8::decode(text, at).codepoint)) {
    at = text::Utf8::next(text, at);
  }
  while (at < text.size() &&
         word_character(text::Utf8::decode(text, at).codepoint)) {
    at = text::Utf8::next(text, at);
  }
  return at;
}

auto TextEditing::word_start(std::string_view text, std::size_t at) noexcept
    -> std::size_t {
  while (
      at > 0 &&
      !word_character(
          text::Utf8::decode(text, text::Utf8::previous(text, at)).codepoint)) {
    at = text::Utf8::previous(text, at);
  }
  while (
      at > 0 &&
      word_character(
          text::Utf8::decode(text, text::Utf8::previous(text, at)).codepoint)) {
    at = text::Utf8::previous(text, at);
  }
  return at;
}

auto TextEditing::line_start(std::string_view text, std::size_t at) noexcept
    -> std::size_t {
  at = std::min(at, text.size());
  while (at > 0 && text[at - 1] != '\n') {
    --at;
  }
  return at;
}

auto TextEditing::line_end(std::string_view text, std::size_t at) noexcept
    -> std::size_t {
  while (at < text.size() && text[at] != '\n') {
    ++at;
  }
  return at;
}

auto TextEditing::word_at(std::string_view text, std::size_t at) noexcept
    -> Range {
  at = text::Utf8::boundary(text, at);
  const bool inside = at < text.size() &&
                      word_character(text::Utf8::decode(text, at).codepoint);
  const bool after =
      at > 0 &&
      word_character(
          text::Utf8::decode(text, text::Utf8::previous(text, at)).codepoint);
  if (!inside && !after) {
    return Range{.lower = at,
                 .upper = at < text.size() ? text::Utf8::next(text, at) : at};
  }
  std::size_t lower = at;
  while (
      lower > 0 &&
      word_character(text::Utf8::decode(text, text::Utf8::previous(text, lower))
                         .codepoint)) {
    lower = text::Utf8::previous(text, lower);
  }
  std::size_t upper = at;
  while (upper < text.size() &&
         word_character(text::Utf8::decode(text, upper).codepoint)) {
    upper = text::Utf8::next(text, upper);
  }
  return Range{.lower = lower, .upper = upper};
}

auto TextEditing::selected() const -> std::string {
  const Range range = selection_.range();
  return text_.substr(range.lower, range.length());
}

void TextEditing::collapse(std::size_t index,
                           composition::TextSelectionAffinity affinity) {
  index = text::Utf8::boundary(text_, index);
  anchor_ = index;
  selection_ =
      composition::TextSelection{
          composition::TextSelection::InsertionPoint{.index = index}}
          .with_affinity(affinity);
}

void TextEditing::insert(std::string_view typed) {
  std::string accepted;
  accepted.reserve(typed.size());
  for (const char byte : typed) {
    if (byte == '\n' && !multiline_) {
      continue;
    }
    if (byte == '\r') {
      continue;
    }
    accepted.push_back(byte);
  }
  const Range range = selection_.range();
  text_.replace(range.lower, range.length(), accepted);
  collapse(range.lower + accepted.size(),
           composition::TextSelectionAffinity::downstream);
}

auto TextEditing::vertical(std::size_t from, bool down) const -> std::size_t {
  const std::size_t start = line_start(text_, from);
  const std::size_t column = from - start;
  if (down) {
    const std::size_t end = line_end(text_, from);
    if (end >= text_.size()) {
      return text_.size();
    }
    const std::size_t below = end + 1;
    return text::Utf8::boundary(
        text_, std::min(below + column, line_end(text_, below)));
  }
  if (start == 0) {
    return 0;
  }
  const std::size_t above = line_start(text_, start - 1);
  return text::Utf8::boundary(text_, std::min(above + column, start - 1));
}

auto TextEditing::target(Direction direction, Motion motion,
                         std::size_t from) const -> std::size_t {
  const bool forward = direction == Direction::forward;
  switch (direction) {
    case Direction::up:
    case Direction::down:
      if (!multiline_ || motion == Motion::document) {
        return direction == Direction::down ? text_.size() : 0;
      }
      return vertical(from, direction == Direction::down);
    case Direction::backward:
    case Direction::forward:
      break;
  }
  switch (motion) {
    case Motion::character:
      return forward ? text::Utf8::next(text_, from)
                     : text::Utf8::previous(text_, from);
    case Motion::word:
      return forward ? word_end(text_, from) : word_start(text_, from);
    case Motion::line:
      return forward ? line_end(text_, from) : line_start(text_, from);
    case Motion::document:
      return forward ? text_.size() : 0;
  }
  return from;
}

void TextEditing::erase(Direction direction, Motion motion) {
  Range range = selection_.range();
  if (range.empty()) {
    const std::size_t other = target(direction, motion, range.lower);
    range = Range{.lower = std::min(other, range.lower),
                  .upper = std::max(other, range.lower)};
  }
  text_.erase(range.lower, range.length());
  collapse(range.lower, composition::TextSelectionAffinity::downstream);
}

void TextEditing::move(Direction direction, Motion motion, bool extend) {
  const Range range = selection_.range();
  const bool upstream =
      selection_.affinity() == composition::TextSelectionAffinity::upstream;
  const std::size_t active = range.empty() ? range.lower
                             : upstream    ? range.lower
                                           : range.upper;
  if (!extend && !range.empty() && motion == Motion::character &&
      (direction == Direction::backward || direction == Direction::forward)) {
    collapse(direction == Direction::forward ? range.upper : range.lower,
             direction == Direction::forward
                 ? composition::TextSelectionAffinity::downstream
                 : composition::TextSelectionAffinity::upstream);
    return;
  }
  const std::size_t moved = target(direction, motion, active);
  const bool backward_motion =
      direction == Direction::backward || direction == Direction::up;
  if (!extend) {
    collapse(moved, backward_motion
                        ? composition::TextSelectionAffinity::upstream
                        : composition::TextSelectionAffinity::downstream);
    return;
  }
  extend_to(moved);
}

void TextEditing::extend_to(std::size_t index) {
  index = text::Utf8::boundary(text_, index);
  const std::size_t anchor = std::min(anchor_, text_.size());
  const Range range{.lower = std::min(anchor, index),
                    .upper = std::max(anchor, index)};
  selection_ = composition::TextSelection{range}.with_affinity(
      index < anchor ? composition::TextSelectionAffinity::upstream
                     : composition::TextSelectionAffinity::downstream);
}

void TextEditing::place(std::size_t index) {
  collapse(index, composition::TextSelectionAffinity::downstream);
}

void TextEditing::select(Range range) {
  range = range.clamped(text_.size());
  range = Range{.lower = text::Utf8::boundary(text_, range.lower),
                .upper = text::Utf8::boundary(text_, range.upper)};
  anchor_ = range.lower;
  selection_ = composition::TextSelection{range}.with_affinity(
      composition::TextSelectionAffinity::downstream);
}

void TextEditing::select_all() {
  select(Range{.lower = 0, .upper = text_.size()});
}

void TextEditing::select_word() {
  select(word_at(text_, selection_.range().lower));
}

auto TextEditing::apply(const composition::KeyPress& press) -> Outcome {
  using composition::EventModifiers;
  using composition::KeyEquivalent;
  if (press.phase == composition::KeyPress::Phases::up) {
    return Outcome::ignored;
  }
  const bool shift = press.modifiers.contains(EventModifiers::shift);
  const bool command = press.modifiers.contains(EventModifiers::command) ||
                       press.modifiers.contains(EventModifiers::control);
  const bool option = press.modifiers.contains(EventModifiers::option);
  const KeyEquivalent key = press.key;
  if (command) {
    const char32_t letter = key.character();
    switch (letter) {
      case U'a':
        select_all();
        return Outcome::moved;
      case U'c':
        return Outcome::copy;
      case U'x':
        return Outcome::cut;
      case U'v':
        return Outcome::paste;
      case U'b':
        return Outcome::bold;
      case U'i':
        return Outcome::italic;
      case U'u':
        return Outcome::underline;
      case U'f':
        return Outcome::find;
      default:
        break;
    }
  }
  const Motion sideways = command  ? Motion::line
                          : option ? Motion::word
                                   : Motion::character;
  if (key == KeyEquivalent::left_arrow) {
    move(Direction::backward, sideways, shift);
    return Outcome::moved;
  }
  if (key == KeyEquivalent::right_arrow) {
    move(Direction::forward, sideways, shift);
    return Outcome::moved;
  }
  if (key == KeyEquivalent::up_arrow || key == KeyEquivalent::down_arrow) {
    if (!multiline_ && !command && !shift) {
      const std::size_t before = selection_.range().lower;
      move(key == KeyEquivalent::up_arrow ? Direction::up : Direction::down,
           Motion::document, false);
      return selection_.range().lower == before ? Outcome::ignored
                                                : Outcome::moved;
    }
    move(key == KeyEquivalent::up_arrow ? Direction::up : Direction::down,
         command ? Motion::document : Motion::line, shift);
    return Outcome::moved;
  }
  if (key == KeyEquivalent::home || key == KeyEquivalent::end) {
    move(key == KeyEquivalent::home ? Direction::backward : Direction::forward,
         multiline_ ? Motion::line : Motion::document, shift);
    return Outcome::moved;
  }
  if (key == KeyEquivalent::page_up || key == KeyEquivalent::page_down) {
    move(key == KeyEquivalent::page_up ? Direction::backward
                                       : Direction::forward,
         Motion::document, shift);
    return Outcome::moved;
  }
  if (key == KeyEquivalent::delete_key) {
    erase(Direction::backward, sideways);
    return Outcome::edited;
  }
  if (key == KeyEquivalent::delete_forward) {
    erase(Direction::forward, sideways);
    return Outcome::edited;
  }
  if (key == KeyEquivalent::escape) {
    return Outcome::cancel;
  }
  if (key == KeyEquivalent::return_key) {
    if (multiline_ && !command) {
      insert("\n");
      return Outcome::edited;
    }
    return Outcome::submit;
  }
  if (key == KeyEquivalent::tab || command || option) {
    return Outcome::ignored;
  }
  if (!press.characters.empty()) {
    insert(press.characters);
    return Outcome::edited;
  }
  return Outcome::ignored;
}

}
