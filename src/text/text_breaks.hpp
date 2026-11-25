#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace arisa::text {

enum class LineBreak : std::uint8_t {
  none,
  allowed,
  mandatory,
};

class TextBreaks {
 public:
  static auto analyse(std::string_view text, std::string_view language)
      -> TextBreaks;

  auto line(std::size_t offset) const noexcept -> LineBreak;

  auto grapheme(std::size_t offset) const noexcept -> bool;

  static auto forbids_line_start(char32_t codepoint) noexcept -> bool;

  static auto forbids_line_end(char32_t codepoint) noexcept -> bool;

 private:
  TextBreaks(std::vector<LineBreak> lines,
             std::vector<std::uint8_t> graphemes) noexcept;

  static auto apply_kinsoku(std::string_view text, std::vector<LineBreak> lines)
      -> std::vector<LineBreak>;

  std::vector<LineBreak> lines_;
  std::vector<std::uint8_t> graphemes_;
};

}
