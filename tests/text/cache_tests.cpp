#include <text_fixture.hpp>

#include <arisa/text/typesetter.hpp>

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>

namespace arisa::text {

class TextCache : public ::testing::Test {
 protected:
  static auto width(float points) -> LayoutWidth {
    return LayoutWidth::points(points).value();
  }

  static auto unit() -> provider::SurfaceScale {
    return provider::SurfaceScale::unit();
  }
};

TEST_F(TextCache, CountsHitsAndMisses) {
  auto typesetter = Typesetter::create(TextFixture::fonts());
  ASSERT_TRUE(typesetter.has_value());
  const TextStyle style = TextFixture::style("en");
  auto first = typesetter->layout("cache me", style, width(200.0F), unit());
  auto second = typesetter->layout("cache me", style, width(200.0F), unit());
  ASSERT_TRUE(first.has_value() && second.has_value());
  EXPECT_EQ(first->get(), second->get());
  CacheStatistics statistics = typesetter->statistics();
  EXPECT_EQ(statistics.layout_misses, 1U);
  EXPECT_EQ(statistics.layout_hits, 1U);
  EXPECT_EQ(statistics.shaping_misses, 1U);
  auto measured = typesetter->measure("cache me", style, width(200.0F), unit());
  ASSERT_TRUE(measured.has_value());
  EXPECT_EQ(*measured, (*first)->size());
  EXPECT_EQ(typesetter->statistics().layout_hits, 2U);
  auto narrow = typesetter->layout("cache me", style, width(40.0F), unit());
  ASSERT_TRUE(narrow.has_value());
  statistics = typesetter->statistics();
  EXPECT_EQ(statistics.layout_misses, 2U);
  EXPECT_EQ(statistics.shaping_hits, 1U);
  EXPECT_EQ(statistics.shaping_misses, 1U);
  EXPECT_EQ(statistics.layouts, 2U);
  EXPECT_EQ(statistics.shaped, 1U);
}

TEST_F(TextCache, StaysWithinItsLruBound) {
  auto typesetter = Typesetter::create(
      TextFixture::fonts(), TypesetterLimits{.layouts = CacheCapacity::of<2>(),
                                             .shaped = CacheCapacity::of<2>()});
  ASSERT_TRUE(typesetter.has_value());
  const TextStyle style = TextFixture::style("en");
  for (const char* text : {"alpha", "beta", "gamma"}) {
    ASSERT_TRUE(
        typesetter->layout(text, style, width(100.0F), unit()).has_value());
  }
  EXPECT_EQ(typesetter->statistics().layouts, 2U);
  EXPECT_EQ(typesetter->statistics().shaped, 2U);
  ASSERT_TRUE(
      typesetter->layout("gamma", style, width(100.0F), unit()).has_value());
  EXPECT_EQ(typesetter->statistics().layout_hits, 1U);
  ASSERT_TRUE(
      typesetter->layout("alpha", style, width(100.0F), unit()).has_value());
  EXPECT_EQ(typesetter->statistics().layout_misses, 4U);
  EXPECT_EQ(typesetter->statistics().shaping_misses, 4U);
  typesetter->clear();
  EXPECT_EQ(typesetter->statistics().layouts, 0U);
}

TEST_F(TextCache, MeasureMatchesLayout) {
  auto typesetter = Typesetter::create(TextFixture::fonts());
  ASSERT_TRUE(typesetter.has_value());
  const TextStyle style = TextFixture::style("ja");
  const char* text =
      "設定を変更しました。 Settings "
      "saved مرحبا";
  for (const float limit : {30.0F, 80.0F, 150.0F, 400.0F}) {
    for (const float scale : {1.0F, 1.5F}) {
      const auto pixels = provider::SurfaceScale::create(scale).value();
      auto measured = typesetter->measure(text, style, width(limit), pixels);
      auto created = TextLayout::create(TextFixture::fonts(), text, style,
                                        width(limit), pixels);
      ASSERT_TRUE(measured.has_value() && created.has_value());
      EXPECT_EQ(*measured, created->size()) << limit << " @ " << scale;
    }
  }
}

TEST_F(TextCache, TruncatesAtTheTailToFitTheLineLimit) {
  auto typesetter = Typesetter::create(TextFixture::fonts());
  ASSERT_TRUE(typesetter.has_value());
  const TextStyle style = TextFixture::style("en");
  const std::string_view sentence = "a long label that cannot fit in the row";
  auto whole =
      typesetter->truncated(sentence, style, width(1000.0F), unit(), 1);
  ASSERT_TRUE(whole.has_value());
  EXPECT_EQ(*whole, sentence);
  auto cut = typesetter->truncated(sentence, style, width(80.0F), unit(), 1);
  ASSERT_TRUE(cut.has_value());
  EXPECT_TRUE(cut->ends_with("\u2026"));
  EXPECT_LT(cut->size(), sentence.size());
  EXPECT_TRUE(sentence.starts_with(cut->substr(0, cut->size() - 3)));
  auto laid = typesetter->layout(*cut, style, width(80.0F), unit());
  ASSERT_TRUE(laid.has_value());
  EXPECT_EQ((*laid)->lines().size(), 1U);
  EXPECT_LE((*laid)->size().width, 80.01F);
}

}
