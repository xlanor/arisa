#include <arisa/components/attributed_string.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/text_attributes.hpp>
#include <arisa/text/typeface.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/typography.hpp>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace arisa::components {

class AttributedStringTest : public ::testing::Test {
 protected:
  using Attributes = AttributedString::Attributes;

  static auto at(const AttributedString& text, std::string_view word)
      -> Attributes {
    const std::size_t found = text.text().find(word);
    EXPECT_NE(found, std::string_view::npos) << word;
    return text.attributes(static_cast<std::uint32_t>(found));
  }
};

TEST_F(AttributedStringTest, AppendMergesNeighbouringRuns) {
  AttributedString text{"a"};
  text.append("b", Attributes{.italic = true});
  text.append("c", Attributes{.italic = true});
  text.append("d");
  EXPECT_EQ(text.text(), "abcd");
  ASSERT_EQ(text.runs().runs().size(), 1U);
  EXPECT_EQ(text.runs().runs()[0].start, 1U);
  EXPECT_EQ(text.runs().runs()[0].end, 3U);
}

TEST_F(AttributedStringTest, InksAreSharedBetweenRuns) {
  AttributedString text;
  text.append("one", Attributes{.foreground = theme::ColourRole::accent});
  text.append(" ");
  text.append("two", Attributes{.foreground = theme::ColourRole::accent});
  EXPECT_EQ(text.inks().size(), 1U);
  EXPECT_EQ(text.runs().runs().size(), 2U);
}

TEST_F(AttributedStringTest, MarkdownCoversTheInlineSubset) {
  const AttributedString text = AttributedString::markdown(
      "**bold** *italic* ***both*** `code` ~~gone~~ "
      "[link](https://example.com) _under_ __strong__");
  EXPECT_EQ(text.text(), "bold italic both code gone link under strong");
  EXPECT_EQ(at(text, "bold").weight, theme::Font::Weight::bold);
  EXPECT_FALSE(at(text, "bold").italic);
  EXPECT_TRUE(at(text, "italic").italic);
  EXPECT_TRUE(at(text, "both").italic);
  EXPECT_EQ(at(text, "both").weight, theme::Font::Weight::bold);
  EXPECT_EQ(at(text, "code").design, theme::Font::Design::monospaced);
  EXPECT_TRUE(at(text, "gone").strikethrough);
  EXPECT_EQ(at(text, "link").link,
            std::optional<std::string>{"https://example.com"});
  EXPECT_TRUE(at(text, "under").italic);
  EXPECT_EQ(at(text, "strong").weight, theme::Font::Weight::bold);
  EXPECT_EQ(at(text, " ").weight, std::nullopt);
}

TEST_F(AttributedStringTest, UnmatchedMarkdownStaysLiteral) {
  EXPECT_EQ(AttributedString::markdown("a *b").text(), "a *b");
  EXPECT_EQ(AttributedString::markdown("2 * 3 * 4").text(), "2 * 3 * 4");
  EXPECT_EQ(AttributedString::markdown("snake_case_name").text(),
            "snake_case_name");
  EXPECT_EQ(AttributedString::markdown("[not a link]").text(), "[not a link]");
  EXPECT_EQ(AttributedString::markdown("`open").text(), "`open");
  EXPECT_EQ(AttributedString::markdown("\\*literal\\*").text(), "*literal*");
  EXPECT_EQ(AttributedString::markdown("~single~").text(), "~single~");
  EXPECT_TRUE(AttributedString::markdown("a *b").runs().empty());
}

TEST_F(AttributedStringTest, MarkdownNestsEmphasisInsideLinks) {
  const AttributedString text =
      AttributedString::markdown("see [the **new** guide](https://x.dev) now");
  EXPECT_EQ(text.text(), "see the new guide now");
  EXPECT_EQ(at(text, "new").weight, theme::Font::Weight::bold);
  EXPECT_EQ(at(text, "new").link, std::optional<std::string>{"https://x.dev"});
  EXPECT_EQ(at(text, "guide").link,
            std::optional<std::string>{"https://x.dev"});
  ASSERT_EQ(text.links().size(), 1U);
  EXPECT_EQ(text.links()[0].destination, "https://x.dev");
  EXPECT_FALSE(at(text, "now").link.has_value());
}

TEST_F(AttributedStringTest, DeepMarkdownIsBounded) {
  std::string source;
  for (int index = 0; index < 5000; ++index) {
    source += "*[";
  }
  const AttributedString text = AttributedString::markdown(source);
  EXPECT_LE(text.text().size(), source.size());
  EXPECT_FALSE(text.text().empty());
}

TEST_F(AttributedStringTest, CasingKeepsRunsOnTheirWords) {
  const AttributedString text = AttributedString::markdown("straße **bold**")
                                    .cased(composition::TextCase::uppercase);
  EXPECT_EQ(text.text(), "STRASSE BOLD");
  EXPECT_EQ(at(text, "BOLD").weight, theme::Font::Weight::bold);
  EXPECT_FALSE(at(text, "STRASSE").weight.has_value());
}

TEST_F(AttributedStringTest, ApplyingFillsOnlyUnsetAttributes) {
  AttributedString text;
  text.append("own", Attributes{.foreground = theme::ColourRole::accent});
  text.append(" plain");
  const AttributedString applied = text.applying(
      Attributes{.italic = true, .foreground = theme::ColourRole::label});
  EXPECT_TRUE(at(applied, "own").italic);
  EXPECT_EQ(at(applied, "own").foreground,
            std::optional<theme::ColourValue>{theme::ColourRole::accent});
  EXPECT_EQ(at(applied, "plain").foreground,
            std::optional<theme::ColourValue>{theme::ColourRole::label});
}

TEST_F(AttributedStringTest, DesignsMapToTypefaces) {
  EXPECT_EQ(AttributedString::typeface(theme::Font::Design::monospaced),
            text::Typeface::monospaced);
  EXPECT_EQ(AttributedString::typeface(theme::Font::Design::serif),
            text::Typeface::serif);
  EXPECT_EQ(AttributedString::typeface(theme::Font::Design::rounded),
            text::Typeface::rounded);
}

TEST_F(AttributedStringTest, TextStylesCarryFaceAttributes) {
  const theme::Theme theme = theme::Theme::defaults();
  composition::TextAttributes attributes;
  attributes.italic = true;
  attributes.design = theme::Font::Design::serif;
  attributes.width = theme::Font::Width::condensed;
  attributes.scale = composition::TextScale::secondary;
  attributes.underline.active = true;
  const text::TextStyle plain =
      Text::text_style(theme, theme::Font::body, text::Locale::undetermined(),
                       std::nullopt, composition::TextAttributes{},
                       composition::LayoutDirection::left_to_right);
  const text::TextStyle styled = Text::text_style(
      theme, theme::Font::body, text::Locale::undetermined(), std::nullopt,
      attributes, composition::LayoutDirection::left_to_right);
  EXPECT_TRUE(styled.italic);
  EXPECT_EQ(styled.typeface, text::Typeface::serif);
  EXPECT_FLOAT_EQ(styled.stretch, theme::Font::condensed_width);
  EXPECT_TRUE(styled.underline);
  EXPECT_NEAR(styled.size.value(), plain.size.value() * Text::secondary_scale,
              0.001F);
  EXPECT_EQ(styled.line_height, plain.line_height);
  const text::TextStyle custom =
      Text::text_style(theme, theme::Font::custom("Lettering", 30.0F),
                       text::Locale::undetermined());
  EXPECT_EQ(custom.name, std::optional{text::FontName{"Lettering"}});
  EXPECT_FLOAT_EQ(custom.size.value(), 30.0F);
}

}
