#include <text_fixture.hpp>

#include <arisa/text/line_height.hpp>
#include <arisa/text/line_pattern.hpp>
#include <arisa/text/small_caps.hpp>
#include <arisa/text/text_attachment.hpp>
#include <arisa/text/text_run.hpp>
#include <arisa/text/text_runs.hpp>
#include <arisa/text/typesetting_language.hpp>
#include <arisa/text/writing_direction.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::text {

class RunAttributesTest : public ::testing::Test, protected TextFixture {
 protected:
  static auto run(std::uint32_t start, std::uint32_t end) -> TextRun {
    return TextRun{.start = start, .end = end};
  }

  static auto styled(TextStyle style, std::vector<TextRun> runs) -> TextStyle {
    style.runs = TextRuns{std::move(runs)};
    return style;
  }
};

TEST_F(RunAttributesTest, ARunWithItsOwnSizeGrowsTheLineAndItsGlyphs) {
  const TextStyle plain = style("en", 17.0F);
  TextRun big = run(0, 3);
  big.size = PointSize::create(34.0F);
  const TextLayout mixed = layout("Big small", styled(plain, {big}));
  const TextLayout small = layout("Big small", plain);
  EXPECT_GT(mixed.size().height, small.size().height * 1.6F);
  EXPECT_GT(mixed.size().width, small.size().width);
  ASSERT_GE(mixed.runs().size(), 2U);
  EXPECT_EQ(mixed.runs().front().size.value(), 34.0F);
  EXPECT_EQ(mixed.runs().back().size.value(), 17.0F);
}

TEST_F(RunAttributesTest, KernTrackingAndBaselinePerRun) {
  const TextStyle plain = style("en", 17.0F);
  TextRun kerned = run(0, 6);
  kerned.kerning = 4.0F;
  EXPECT_NEAR(layout("Kerned", styled(plain, {kerned})).size().width,
              layout("Kerned", plain).size().width + 24.0F, 0.5F);
  TextRun tracked = run(0, 3);
  tracked.tracking = 2.0F;
  EXPECT_NEAR(layout("abcdef", styled(plain, {tracked})).size().width,
              layout("abcdef", plain).size().width + 6.0F, 0.5F);
  TextRun raised = run(0, 6);
  raised.baseline_offset = 6.0F;
  const TextLayout up = layout("Raised", styled(plain, {raised}));
  const TextLayout level = layout("Raised", plain);
  EXPECT_GT(up.size().height, level.size().height + 5.0F);
  EXPECT_LT(glyph_at(up, 0)->y, glyph_at(level, 0)->y + 0.5F);
}

TEST_F(RunAttributesTest, LineHeightRulesMatchTheMeasuredFactors) {
  EXPECT_EQ(LineHeight::multiple(1.5F).points(20.0F, 24.0F),
            std::optional<float>{30.0F});
  EXPECT_EQ(LineHeight::tight.points(20.0F, 24.0F),
            std::optional<float>{20.0F});
  EXPECT_EQ(LineHeight::loose.points(20.0F, 24.0F),
            std::optional<float>{30.0F});
  EXPECT_EQ(LineHeight::exact(30.0F).points(20.0F, 24.0F),
            std::optional<float>{30.0F});
  EXPECT_EQ(LineHeight::leading(4.0F).points(20.0F, 24.0F),
            std::optional<float>{28.0F});
  EXPECT_FALSE(LineHeight::normal.points(20.0F, 24.0F).has_value());
  EXPECT_FALSE(LineHeight::variable.points(20.0F, 24.0F).has_value());
  TextStyle ruled = style("en", 20.0F);
  ruled.line_rule = LineHeight::exact(30.0F);
  EXPECT_NEAR(layout("One\nTwo\nThree", ruled).size().height, 90.0F, 0.5F);
  TextRun tall = run(0, 3);
  tall.line_height = LineHeight::multiple(2.0F);
  const TextLayout lines =
      layout("One\nTwo", styled(style("en", 20.0F), {tall}));
  ASSERT_EQ(lines.lines().size(), 2U);
  EXPECT_NEAR(lines.lines()[0].height, 40.0F, 0.5F);
  EXPECT_LT(lines.lines()[1].height, 30.0F);
}

TEST_F(RunAttributesTest, AParagraphAlignmentRunAlignsItsLine) {
  TextStyle wide = style("en", 20.0F);
  TextRun centred = run(4, 8);
  centred.alignment = TextAlignment::center;
  const TextLayout laid =
      layout("Long first line\nmid", styled(wide, {centred}));
  ASSERT_EQ(laid.lines().size(), 2U);
  EXPECT_EQ(laid.lines()[0].x, 0.0F);
  TextRun ended = run(16, 19);
  ended.alignment = TextAlignment::opposite;
  const TextLayout trailing =
      layout("Long first line\nend", styled(wide, {ended}));
  EXPECT_NEAR(trailing.lines()[1].x + trailing.lines()[1].width,
              trailing.size().width, 0.5F);
}

TEST_F(RunAttributesTest, SmallCapitalsScaleLowercaseLetters) {
  TextStyle plain = style("en", 40.0F);
  TextStyle capitals = plain;
  capitals.small_caps = SmallCaps::lowercase;
  const TextLayout lower = layout("h", plain);
  const TextLayout small = layout("h", capitals);
  const TextLayout upper = layout("H", plain);
  ASSERT_FALSE(small.runs().empty());
  EXPECT_FLOAT_EQ(small.runs().front().size.value(),
                  40.0F * SmallCapitals::scale);
  EXPECT_NE(small.glyphs().front().glyph, lower.glyphs().front().glyph);
  EXPECT_NEAR(small.size().width, upper.size().width * SmallCapitals::scale,
              1.0F);
  TextStyle both = plain;
  both.small_caps = SmallCaps::all;
  EXPECT_FLOAT_EQ(layout("H", both).runs().front().size.value(),
                  40.0F * SmallCapitals::scale);
  TextRun capped = run(0, 1);
  capped.small_caps = SmallCaps::uppercase;
  EXPECT_FLOAT_EQ(
      layout("Hh", styled(plain, {capped})).runs().front().size.value(),
      40.0F * SmallCapitals::scale);
  EXPECT_TRUE(SmallCapitals::lowers(SmallCaps::all));
  EXPECT_FALSE(SmallCapitals::uppers(SmallCaps::lowercase));
}

TEST_F(RunAttributesTest, AnAttachmentReservesItsBoxOnTheBaseline) {
  const std::string text = "A￼B";
  TextRun image = run(1, 4);
  image.attachment = TextAttachment{.width = 30.0F, .height = 40.0F};
  image.image = 1;
  const TextLayout laid = layout(text, styled(style("en", 17.0F), {image}));
  const TextLayout plain = layout("AB", style("en", 17.0F));
  EXPECT_NEAR(laid.size().width, plain.size().width + 30.0F, 0.5F);
  EXPECT_GT(laid.size().height, 40.0F);
}

TEST_F(RunAttributesTest, WritingDirectionAndTypesettingLanguage) {
  TextStyle natural = style("en", 17.0F);
  const TextLayout content = layout("שלום abc", natural);
  EXPECT_TRUE(content.lines().front().right_to_left);
  TextStyle forced = natural;
  forced.direction = WritingDirection::left_to_right;
  EXPECT_FALSE(layout("שלום abc", forced).lines().front().right_to_left);
  TextStyle right = natural;
  right.direction = WritingDirection::right_to_left;
  EXPECT_TRUE(layout("abc", right).lines().front().right_to_left);
  TextStyle japanese = style("en", 17.0F);
  japanese.language = Locale::create("ja");
  const TextLayout shaped = layout("直", japanese);
  EXPECT_FALSE(shaped.glyphs().empty());
  EXPECT_EQ(TypesettingLanguage::explicit_(*Locale::create("ja")).language(),
            Locale::create("ja"));
  EXPECT_FALSE(TypesettingLanguage::automatic.language().has_value());
}

TEST_F(RunAttributesTest, DashPatternsScaleWithTheFontSize) {
  EXPECT_TRUE(LineDashes::of(LinePattern::solid).empty());
  EXPECT_EQ(LineDashes::of(LinePattern::dot).size(), 2U);
  EXPECT_EQ(LineDashes::of(LinePattern::dash).front(), LineDashes::dash);
  EXPECT_EQ(LineDashes::of(LinePattern::dash_dot).size(), 4U);
  EXPECT_EQ(LineDashes::of(LinePattern::dash_dot_dot).size(), 6U);
  EXPECT_FLOAT_EQ(LineDashes::dash * 20.0F, 20.0F);
  EXPECT_FLOAT_EQ(LineDashes::dot * 20.0F, 6.0F);
  EXPECT_FLOAT_EQ(LineDashes::dash_gap * 20.0F, 10.0F);
}

TEST_F(RunAttributesTest, ItalicGreekAndCyrillicUseTheItalicFace) {
  TextStyle slanted = style("en", 20.0F);
  slanted.italic = true;
  for (const std::string_view sample :
       {std::string_view{"Καλημέρα"}, std::string_view{"Здравствуйте"},
        std::string_view{"Latin"}}) {
    const TextLayout laid = layout(sample, slanted);
    ASSERT_FALSE(laid.runs().empty()) << sample;
    const auto italic =
        std::ranges::find_if(fonts().entries(), [&](const FontEntry& entry) {
          return entry.face == laid.runs().front().face;
        });
    ASSERT_NE(italic, fonts().entries().end()) << sample;
    EXPECT_TRUE(italic->italic) << sample;
    EXPECT_EQ(laid.runs().front().face->options().slant, 0.0F) << sample;
  }
}

TEST_F(RunAttributesTest, RunsHashTheirNewAttributes) {
  TextRun one = run(0, 2);
  TextRun two = one;
  two.background = 1;
  EXPECT_NE(TextRuns{std::vector<TextRun>{one}}.hash(),
            TextRuns{std::vector<TextRun>{two}}.hash());
  two = one;
  two.underline_pattern = LinePattern::dash;
  EXPECT_NE(TextRuns{std::vector<TextRun>{one}}.hash(),
            TextRuns{std::vector<TextRun>{two}}.hash());
  two = one;
  two.size = PointSize::create(30.0F);
  EXPECT_NE(TextRuns{std::vector<TextRun>{one}},
            TextRuns{std::vector<TextRun>{two}});
}

}
