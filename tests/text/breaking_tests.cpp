#include <text_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::text {

class TextBreaking : public ::testing::Test {
 protected:
  static auto line_text(const std::string& text, const TextLine& line)
      -> std::string {
    return text.substr(line.text_start, line.text_end - line.text_start);
  }

  static auto width_of(std::string_view text, const TextStyle& style) -> float {
    return TextFixture::layout(text, style).size().width;
  }

  static auto starts_with_any(std::string_view text,
                              std::span<const std::string_view> prefixes)
      -> bool {
    return std::ranges::any_of(prefixes, [text](std::string_view prefix) {
      return text.starts_with(prefix);
    });
  }

  static auto ends_with_any(std::string_view text,
                            std::span<const std::string_view> suffixes)
      -> bool {
    return std::ranges::any_of(suffixes, [text](std::string_view suffix) {
      return text.ends_with(suffix);
    });
  }
};

TEST_F(TextBreaking, CjkBreaksBetweenCharacters) {
  const std::string text =
      "日本語の文章を正しく折り"
      "返す試験です";
  const TextStyle style = TextFixture::style("ja");
  const float limit = (style.size.value() * 5.0F) + 0.5F;
  const TextLayout layout = TextFixture::layout(text, style, limit);
  ASSERT_GE(layout.lines().size(), 3U);
  std::string joined;
  for (const TextLine& line : layout.lines()) {
    EXPECT_LE(line.width, limit + 0.01F);
    EXPECT_GE(line.width, style.size.value() * 3.0F);
    joined += line_text(text, line);
  }
  EXPECT_EQ(joined, text);
}

TEST_F(TextBreaking, KinsokuKeepsClosingPunctuationOffLineStarts) {
  const TextStyle style = TextFixture::style("ja");
  const std::string head = "設定を変更しました";
  const std::string text = head + "。次へ";
  const TextLayout layout =
      TextFixture::layout(text, style, width_of(head, style) + 0.5F);
  ASSERT_EQ(layout.lines().size(), 2U);
  EXPECT_EQ(line_text(text, layout.lines()[1]).substr(0, 6), "た。");
}

TEST_F(TextBreaking, KinsokuHoldsAcrossWidths) {
  const std::string text =
      "設定を変更しました。「次"
      "へ」を押してください。ゲ"
      "ーム（新作）、ちょっと待"
      "ってね！";
  constexpr std::array<std::string_view, 9> no_start{
      "。", "、", "）", "」", "！", "ー", "っ", "ょ", "，"};
  constexpr std::array<std::string_view, 2> no_end{"「", "（"};
  const TextStyle style = TextFixture::style("ja");
  for (int characters = 3; characters <= 14; ++characters) {
    const float limit =
        (style.size.value() * static_cast<float>(characters)) + 0.5F;
    const TextLayout layout = TextFixture::layout(text, style, limit);
    for (std::size_t index = 0; index < layout.lines().size(); ++index) {
      const std::string line = line_text(text, layout.lines()[index]);
      if (index > 0) {
        EXPECT_FALSE(starts_with_any(line, no_start))
            << characters << " characters, line " << index;
      }
      EXPECT_FALSE(ends_with_any(line, no_end))
          << characters << " characters, line " << index;
    }
  }
}

TEST_F(TextBreaking, LatinBreaksAtSpaces) {
  const TextStyle style = TextFixture::style("en");
  const std::string text = "The quick brown fox";
  const TextLayout layout = TextFixture::layout(
      text, style, width_of("The quick brown", style) + 0.5F);
  ASSERT_EQ(layout.lines().size(), 2U);
  EXPECT_EQ(line_text(text, layout.lines()[0]), "The quick brown ");
  EXPECT_EQ(line_text(text, layout.lines()[1]), "fox");
  EXPECT_NEAR(layout.lines()[0].width, width_of("The quick brown", style),
              0.01F);
}

TEST_F(TextBreaking, LongWordsOverflowAtGraphemes) {
  const TextStyle style = TextFixture::style("en");
  const std::string text = "Supercalifragilistic";
  const float limit = width_of("Super", style) + 0.5F;
  const TextLayout layout = TextFixture::layout(text, style, limit);
  ASSERT_GE(layout.lines().size(), 3U);
  std::string joined;
  for (const TextLine& line : layout.lines()) {
    EXPECT_GT(line.text_end, line.text_start);
    EXPECT_LE(line.width, limit + 0.01F);
    joined += line_text(text, line);
  }
  EXPECT_EQ(joined, text);
  EXPECT_EQ(line_text(text, layout.lines()[0]), "Super");
}

TEST_F(TextBreaking, SingleGraphemeWiderThanTheLimitStillFits) {
  const TextLayout layout =
      TextFixture::layout("WW", TextFixture::style("en"), 1.0F);
  ASSERT_EQ(layout.lines().size(), 2U);
  EXPECT_GT(layout.lines()[0].width, 1.0F);
}

TEST_F(TextBreaking, MandatoryBreaksAndTrailingNewline) {
  const std::string text = "one\ntwo\n";
  const TextLayout layout = TextFixture::layout(text, TextFixture::style("en"));
  ASSERT_EQ(layout.lines().size(), 3U);
  EXPECT_EQ(line_text(text, layout.lines()[0]), "one\n");
  EXPECT_EQ(line_text(text, layout.lines()[1]), "two\n");
  EXPECT_EQ(layout.lines()[2].text_start, layout.lines()[2].text_end);
  EXPECT_GT(layout.lines()[2].height, 0.0F);
}

TEST_F(TextBreaking, FuzzedInputsBreakWithinTheirOwnBuffer) {
  const std::filesystem::path regressions =
      std::filesystem::path{ARISA_TEST_FIXTURES}.parent_path().parent_path() /
      "fuzz" / "regressions" / "text";
  int replayed = 0;
  for (const auto& entry : std::filesystem::directory_iterator{regressions}) {
    const std::vector<std::byte> raw = TextFixture::read(entry.path());
    const std::string text{reinterpret_cast<const char*>(raw.data()),
                           raw.size()};
    for (const std::string_view locale : {"en-US", "he", "ja-JP", "th"}) {
      for (const float width : {24.0F, 120.0F}) {
        const auto laid = TextLayout::create(
            TextFixture::fonts(), text, TextFixture::style(locale),
            LayoutWidth::points(width).value(), provider::SurfaceScale::unit());
        if (laid.has_value()) {
          EXPECT_FALSE(laid->lines().empty()) << entry.path();
        }
      }
    }
    ++replayed;
  }
  EXPECT_GE(replayed, 1);
}

}
