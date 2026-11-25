#include <text_fixture.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <string>

namespace arisa::text {

TEST(TextLayoutBasics, EmptyTextHasOneLine) {
  const TextLayout layout = TextFixture::layout("", TextFixture::style("en"));
  ASSERT_EQ(layout.lines().size(), 1U);
  EXPECT_EQ(layout.size().width, 0.0F);
  EXPECT_GT(layout.size().height, 0.0F);
  EXPECT_TRUE(layout.glyphs().empty());
}

TEST(TextLayoutBasics, RejectsInvalidUtf8) {
  const std::string broken{"ab\xC3", 3};
  auto layout = TextLayout::create(
      TextFixture::fonts(), broken, TextFixture::style("en"),
      LayoutWidth::unbounded(), provider::SurfaceScale::unit());
  ASSERT_FALSE(layout.has_value());
  EXPECT_EQ(layout.error().code(), TextErrorCode::invalid_text);
}

TEST(TextLayoutBasics, LineHeightSetsTheLineAdvance) {
  TextStyle style = TextFixture::style("en", 13.0F);
  style.line_height = PointSize::of<16.0F>();
  const TextLayout layout = TextFixture::layout("one\ntwo\nthree", style);
  ASSERT_EQ(layout.lines().size(), 3U);
  EXPECT_FLOAT_EQ(layout.size().height, 48.0F);
  EXPECT_FLOAT_EQ(layout.lines()[1].top, 16.0F);
}

TEST(TextLayoutBasics, BaselinesSnapToDevicePixels) {
  TextStyle style = TextFixture::style("en", 13.0F);
  style.line_height = PointSize::of<15.3F>();
  for (const float scale : {1.0F, 1.5F}) {
    const TextLayout layout =
        TextFixture::layout("a\nb\nc\nd", style, std::nullopt, scale);
    for (const TextLine& line : layout.lines()) {
      const float pixels = line.baseline * scale;
      EXPECT_NEAR(pixels, std::round(pixels), 0.001F);
    }
  }
}

TEST(TextLayoutBasics, TrackingWidensLatinButNotArabic) {
  TextStyle tracked = TextFixture::style("en");
  tracked.tracking = 2.0F;
  const float plain =
      TextFixture::layout("abcd", TextFixture::style("en")).size().width;
  EXPECT_NEAR(TextFixture::layout("abcd", tracked).size().width, plain + 8.0F,
              0.01F);
  const std::string arabic = "مرحبا";
  const float joined =
      TextFixture::layout(arabic, TextFixture::style("ar")).size().width;
  TextStyle tracked_arabic = TextFixture::style("ar");
  tracked_arabic.tracking = 2.0F;
  EXPECT_FLOAT_EQ(TextFixture::layout(arabic, tracked_arabic).size().width,
                  joined);
}

TEST(TextLayoutBasics, CenterAlignmentCentresShortLines) {
  TextStyle style = TextFixture::style("en");
  style.alignment = TextAlignment::center;
  const TextLayout layout = TextFixture::layout("a long first line\nb", style);
  ASSERT_EQ(layout.lines().size(), 2U);
  const TextLine& line = layout.lines()[1];
  EXPECT_NEAR(line.x, (layout.size().width - line.width) / 2.0F, 0.01F);
  EXPECT_GE(layout.glyphs(layout.runs()[line.first_run]).front().x, line.x);
}

TEST(TextLayoutBasics, LayoutPositionsDoNotDependOnScale) {
  const std::string text = "Scale independent layout";
  const TextLayout handheld =
      TextFixture::layout(text, TextFixture::style("en"), 120.0F, 1.0F);
  const TextLayout docked =
      TextFixture::layout(text, TextFixture::style("en"), 120.0F, 1.5F);
  ASSERT_EQ(handheld.glyphs().size(), docked.glyphs().size());
  ASSERT_EQ(handheld.lines().size(), docked.lines().size());
  for (std::size_t index = 0; index < handheld.glyphs().size(); ++index) {
    EXPECT_FLOAT_EQ(handheld.glyphs()[index].x, docked.glyphs()[index].x);
  }
}

TEST(TextLocale, NormalisesTags) {
  const auto locale = Locale::create("zh_hant_tw");
  ASSERT_TRUE(locale.has_value());
  EXPECT_EQ(locale->tag(), "zh-Hant-TW");
  EXPECT_EQ(locale->language(), "zh");
  EXPECT_EQ(locale->script(), "Hant");
  EXPECT_EQ(locale->region(), "TW");
  EXPECT_EQ(Locale::create("EN-us")->tag(), "en-US");
  EXPECT_FALSE(Locale::create("").has_value());
  EXPECT_FALSE(Locale::create("1x").has_value());
  EXPECT_FALSE(Locale::create("en--US").has_value());
  EXPECT_EQ(Locale::undetermined().tag(), "und");
}

TEST(TextLocale, KnowsRightToLeftLanguages) {
  EXPECT_TRUE(Locale::create("ar-EG")->right_to_left());
  EXPECT_TRUE(Locale::create("he")->right_to_left());
  EXPECT_FALSE(Locale::create("ja")->right_to_left());
  EXPECT_FALSE(Locale::create("az-Latn")->right_to_left());
}

TEST(TextLocale, PrefersTheMatchingCjkFace) {
  EXPECT_EQ(FontCollection::preferred_cjk(*Locale::create("ja-JP")),
            FontRole::japanese);
  EXPECT_EQ(FontCollection::preferred_cjk(*Locale::create("zh-Hans")),
            FontRole::chinese_simplified);
  EXPECT_EQ(FontCollection::preferred_cjk(*Locale::create("zh-HK")),
            FontRole::chinese_traditional);
  EXPECT_EQ(FontCollection::preferred_cjk(*Locale::create("ko")),
            FontRole::korean);
  EXPECT_FALSE(FontCollection::preferred_cjk(*Locale::create("fr")));
}

TEST(TextValues, RejectInvalidSizesAndWidths) {
  EXPECT_FALSE(PointSize::create(0.0F).has_value());
  EXPECT_FALSE(PointSize::create(-3.0F).has_value());
  EXPECT_FALSE(PointSize::create(10000.0F).has_value());
  EXPECT_FALSE(LayoutWidth::points(-1.0F).has_value());
  EXPECT_TRUE(LayoutWidth::points(0.0F).has_value());
  EXPECT_FALSE(LayoutWidth::unbounded().bounded());
}

}
