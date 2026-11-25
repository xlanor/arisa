#include <text_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::text {

TEST(TextShaping, InterArrowLigatureComesFromItsOwnCalt) {
  const TextLayout layout = TextFixture::layout("->", TextFixture::style("en"));
  ASSERT_EQ(layout.runs().size(), 1U);
  const GlyphRun& run = layout.runs().front();
  ASSERT_EQ(run.glyph_count, 1U);
  EXPECT_EQ(layout.glyphs(run).front().glyph, run.face->glyph(U'→'));
}

TEST(TextShaping, KerningTightensAv) {
  const TextStyle style = TextFixture::style("en");
  const float pair = TextFixture::layout("AV", style).size().width;
  const float apart = TextFixture::layout("A", style).size().width +
                      TextFixture::layout("V", style).size().width;
  EXPECT_LT(pair, apart - 0.1F);
}

TEST(TextShaping, ArabicTakesContextualForms) {
  const TextStyle style = TextFixture::style("ar");
  const TextLayout layout = TextFixture::layout("\u0628\u0628\u0628", style);
  const auto face = TextFixture::script_face("NotoSansArabic-Subset.ttf");
  ASSERT_NE(face, nullptr);
  ASSERT_EQ(layout.runs().size(), 1U);
  EXPECT_EQ(layout.runs().front().face, face);
  EXPECT_TRUE(layout.runs().front().right_to_left);
  const auto names = [&face](const TextLayout& shaped, std::uint32_t cluster) {
    std::vector<std::string> found;
    for (const PositionedGlyph& glyph : shaped.glyphs()) {
      if (glyph.cluster == cluster) {
        found.push_back(face->glyph_name(glyph.glyph).value_or(""));
      }
    }
    std::ranges::sort(found);
    return found;
  };
  const auto body = [](const std::vector<std::string>& glyphs) {
    const auto found = std::ranges::find_if(
        glyphs,
        [](const std::string& name) { return name.starts_with("uni066E"); });
    return found == glyphs.end() ? std::string{} : *found;
  };
  const std::vector<std::string> initial = names(layout, 0);
  const std::vector<std::string> medial = names(layout, 2);
  const std::vector<std::string> final_form = names(layout, 4);
  const TextLayout alone = TextFixture::layout("\u0628", style);
  const std::vector<std::string> isolated = names(alone, 0);
  EXPECT_TRUE(body(initial).starts_with("uni066E.init"));
  EXPECT_TRUE(body(medial).starts_with("uni066E.medi"));
  EXPECT_TRUE(body(final_form).starts_with("uni066E.fina"));
  EXPECT_EQ(body(isolated), "uni066E");
  const auto dot = [](const std::vector<std::string>& glyphs) {
    return std::ranges::find_if(glyphs, [](const std::string& name) {
             return !name.starts_with("uni066E");
           }) != glyphs.end();
  };
  EXPECT_TRUE(dot(initial) && dot(medial) && dot(final_form) && dot(isolated));
  const auto initial_glyph = TextFixture::glyph_at(layout, 0);
  const auto final_glyph = TextFixture::glyph_at(layout, 4);
  ASSERT_TRUE(initial_glyph.has_value() && final_glyph.has_value());
  EXPECT_GT(initial_glyph->x, final_glyph->x);
}

TEST(TextShaping, DevanagariReordersPreBaseMatra) {
  const TextLayout layout = TextFixture::layout("कि", TextFixture::style("hi"));
  const auto face = TextFixture::script_face("NotoSansDevanagari-Subset.ttf");
  ASSERT_EQ(layout.runs().size(), 1U);
  EXPECT_EQ(layout.runs().front().face, face);
  ASSERT_EQ(layout.glyphs().size(), 2U);
  EXPECT_NE(layout.glyphs()[0].glyph, face->glyph(U'क'));
  EXPECT_EQ(layout.glyphs()[1].glyph, face->glyph(U'क'));
  EXPECT_LT(layout.glyphs()[0].x, layout.glyphs()[1].x);
}

TEST(TextShaping, DevanagariFormsConjuncts) {
  const TextLayout layout = TextFixture::layout("क्ष", TextFixture::style("hi"));
  const auto face = TextFixture::script_face("NotoSansDevanagari-Subset.ttf");
  EXPECT_LT(layout.glyphs().size(), 3U);
  for (const PositionedGlyph& glyph : layout.glyphs()) {
    EXPECT_NE(glyph.glyph, face->glyph(U'्'));
    EXPECT_NE(glyph.glyph, face->glyph(U'क'));
  }
}

TEST(TextShaping, ThaiCombiningMarksStackWithoutAdvance) {
  const TextLayout layout = TextFixture::layout("ตั้ง", TextFixture::style("th"));
  const auto face = TextFixture::script_face("NotoSansThai-Subset.ttf");
  ASSERT_EQ(layout.runs().size(), 1U);
  EXPECT_EQ(layout.runs().front().face, face);
  ASSERT_EQ(layout.glyphs().size(), 4U);
  const auto marks = std::ranges::count_if(
      layout.glyphs(),
      [](const PositionedGlyph& glyph) { return glyph.advance == 0.0F; });
  EXPECT_EQ(marks, 2);
}

}
