#include <text_fixture.hpp>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace arisa::text {

class TextBidi : public ::testing::Test {
 protected:
  static auto x(const TextLayout& layout, const std::string& text,
                std::string_view part) -> float {
    const std::size_t offset = text.find(part);
    EXPECT_NE(offset, std::string::npos) << part;
    const auto glyph = TextFixture::glyph_at(layout, offset);
    EXPECT_TRUE(glyph.has_value()) << part;
    return glyph.has_value() ? glyph->x : -1.0F;
  }
};

TEST_F(TextBidi, MixedParagraphReordersRightToLeftRuns) {
  const std::string text = "Hello שלום world مرحبا 123";
  const TextLayout layout = TextFixture::layout(text, TextFixture::style("en"));
  ASSERT_EQ(layout.lines().size(), 1U);
  EXPECT_FALSE(layout.lines().front().right_to_left);
  const float hello = x(layout, text, "H");
  const float shin = x(layout, text, "ש");
  const float final_mem = x(layout, text, "ם");
  const float world = x(layout, text, "w");
  const float meem = x(layout, text, "م");
  const float alef = x(layout, text, "ا");
  const float digits = x(layout, text, "1");
  EXPECT_LT(hello, final_mem);
  EXPECT_LT(final_mem, shin);
  EXPECT_LT(shin, world);
  EXPECT_LT(world, digits);
  EXPECT_LT(digits, alef);
  EXPECT_LT(alef, meem);
}

TEST_F(TextBidi, RightToLeftParagraphStartsOnTheRight) {
  const std::string text = "שלום abc";
  const TextLayout layout = TextFixture::layout(text, TextFixture::style("he"));
  ASSERT_EQ(layout.lines().size(), 1U);
  EXPECT_TRUE(layout.lines().front().right_to_left);
  EXPECT_GT(x(layout, text, "ש"), x(layout, text, "c"));
  EXPECT_LT(x(layout, text, "a"), x(layout, text, "c"));
}

TEST_F(TextBidi, NaturalAlignmentPushesRightToLeftLinesRight) {
  const std::string text = "שלום עולם\nש";
  const TextLayout layout = TextFixture::layout(text, TextFixture::style("he"));
  ASSERT_EQ(layout.lines().size(), 2U);
  const TextLine& short_line = layout.lines()[1];
  EXPECT_NEAR(short_line.x + short_line.width, layout.size().width, 0.01F);
  EXPECT_GT(short_line.x, 0.0F);
}

}
