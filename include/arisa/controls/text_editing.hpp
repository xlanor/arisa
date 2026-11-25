#pragma once

#include <arisa/composition/key_press.hpp>
#include <arisa/composition/text_selection.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace arisa::controls {

class TextEditing {
 public:
  enum class Direction : std::uint8_t {
    backward,
    forward,
    up,
    down,
  };

  enum class Motion : std::uint8_t {
    character,
    word,
    line,
    document,
  };

  enum class Outcome : std::uint8_t {
    ignored,
    edited,
    moved,
    copy,
    cut,
    paste,
    submit,
    cancel,
    bold,
    italic,
    underline,
    find,
  };

  using Range = composition::TextSelection::Range;

  TextEditing(std::string text, composition::TextSelection selection,
              bool multiline);

  auto text() const noexcept -> const std::string& { return text_; }

  auto selection() const noexcept -> const composition::TextSelection& {
    return selection_;
  }

  auto range() const noexcept -> Range { return selection_.range(); }

  auto selected() const -> std::string;

  void insert(std::string_view typed);

  void erase(Direction direction, Motion motion);

  void move(Direction direction, Motion motion, bool extend);

  void select(Range range);

  void select_all();

  void select_word();

  void place(std::size_t index);

  void extend_to(std::size_t index);

  auto apply(const composition::KeyPress& press) -> Outcome;

  static auto word_end(std::string_view text, std::size_t at) noexcept
      -> std::size_t;

  static auto word_start(std::string_view text, std::size_t at) noexcept
      -> std::size_t;

  static auto line_start(std::string_view text, std::size_t at) noexcept
      -> std::size_t;

  static auto line_end(std::string_view text, std::size_t at) noexcept
      -> std::size_t;

  static auto word_at(std::string_view text, std::size_t at) noexcept -> Range;

 private:
  static auto word_character(char32_t codepoint) noexcept -> bool;

  auto target(Direction direction, Motion motion, std::size_t from) const
      -> std::size_t;

  auto vertical(std::size_t from, bool down) const -> std::size_t;

  void collapse(std::size_t index, composition::TextSelectionAffinity affinity);

  std::string text_;
  composition::TextSelection selection_;
  std::size_t anchor_{0};
  bool multiline_{false};
};

}
