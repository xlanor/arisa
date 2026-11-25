#include <arisa/text/utf8.hpp>
#include <text/text_breaks.hpp>

#include <graphemebreak.h>
#include <linebreak.h>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace arisa::text {

TextBreaks::TextBreaks(std::vector<LineBreak> lines,
                       std::vector<std::uint8_t> graphemes) noexcept
    : lines_(std::move(lines)), graphemes_(std::move(graphemes)) {}

auto TextBreaks::analyse(std::string_view text, std::string_view language)
    -> TextBreaks {
  const std::size_t size = text.size();
  std::vector<LineBreak> lines(size + 1, LineBreak::none);
  std::vector<std::uint8_t> graphemes(size + 1, 0);
  graphemes.front() = 1;
  graphemes.back() = 1;
  if (size == 0) {
    return TextBreaks{std::move(lines), std::move(graphemes)};
  }
  const std::string tag{language};
  const char* const lang = tag.empty() || tag == "und" ? nullptr : tag.c_str();
  const auto* bytes =
      static_cast<const utf8_t*>(static_cast<const void*>(text.data()));
  std::vector<char> breaks(size);
  set_linebreaks_utf8(bytes, size, lang, breaks.data());
  for (std::size_t index = 0; index < size; ++index) {
    switch (breaks[index]) {
      case LINEBREAK_MUSTBREAK:
        lines[index + 1] = LineBreak::mandatory;
        break;
      case LINEBREAK_ALLOWBREAK:
        lines[index + 1] = LineBreak::allowed;
        break;
      default:
        break;
    }
  }
  lines.back() = LineBreak::mandatory;
  set_graphemebreaks_utf8(bytes, size, lang, breaks.data());
  for (std::size_t index = 0; index < size; ++index) {
    if (breaks[index] == GRAPHEMEBREAK_BREAK) {
      graphemes[index + 1] = 1;
    }
  }
  return TextBreaks{apply_kinsoku(text, std::move(lines)),
                    std::move(graphemes)};
}

auto TextBreaks::apply_kinsoku(std::string_view text,
                               std::vector<LineBreak> lines)
    -> std::vector<LineBreak> {
  for (std::size_t offset = 1; offset < text.size(); ++offset) {
    if (lines[offset] != LineBreak::allowed) {
      continue;
    }
    const char32_t before =
        Utf8::decode(text, Utf8::previous(text, offset)).codepoint;
    const char32_t after = Utf8::decode(text, offset).codepoint;
    if (forbids_line_start(after) || forbids_line_end(before)) {
      lines[offset] = LineBreak::none;
    }
  }
  return lines;
}

auto TextBreaks::line(std::size_t offset) const noexcept -> LineBreak {
  return offset < lines_.size() ? lines_[offset] : LineBreak::none;
}

auto TextBreaks::grapheme(std::size_t offset) const noexcept -> bool {
  return offset < graphemes_.size() && graphemes_[offset] != 0;
}

auto TextBreaks::forbids_line_start(char32_t codepoint) noexcept -> bool {
  static constexpr std::array<char32_t, 70> forbidden{
      U')',  U']',  U'}',  U'»',  U'’',  U'”',  U'、', U'。', U'〉', U'》',
      U'」', U'』', U'】', U'〕', U'〗', U'〙', U'〛', U'〟', U'｠', U'）',
      U'］', U'｝', U'，', U'．', U'：', U'；', U'？', U'！', U'・', U'‼',
      U'⁇',  U'⁈',  U'⁉',  U'ヽ', U'ヾ', U'ゝ', U'ゞ', U'々', U'〻', U'ー',
      U'ぁ', U'ぃ', U'ぅ', U'ぇ', U'ぉ', U'っ', U'ゃ', U'ゅ', U'ょ', U'ゎ',
      U'ゕ', U'ゖ', U'ァ', U'ィ', U'ゥ', U'ェ', U'ォ', U'ッ', U'ャ', U'ュ',
      U'ョ', U'ヮ', U'ヵ', U'ヶ', U'゛', U'゜', U'〜', U'～', U'｡',  U'､'};
  if (codepoint >= U'ㇰ' && codepoint <= U'ㇿ') {
    return true;
  }
  return std::ranges::contains(forbidden, codepoint);
}

auto TextBreaks::forbids_line_end(char32_t codepoint) noexcept -> bool {
  static constexpr std::array<char32_t, 20> forbidden{
      U'(',  U'[',  U'{',  U'«',  U'‘',  U'“',  U'〈', U'《', U'「', U'『',
      U'【', U'〔', U'〖', U'〘', U'〚', U'〝', U'｟', U'（', U'［', U'｛'};
  return std::ranges::contains(forbidden, codepoint);
}

}
