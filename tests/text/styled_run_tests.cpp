#include <text_fixture.hpp>

#include <arisa/provider/headless/memory_fonts.hpp>
#include <arisa/text/font_name.hpp>
#include <arisa/text/text_run.hpp>
#include <arisa/text/text_runs.hpp>
#include <arisa/text/typeface.hpp>
#include <arisa/text/typesetter.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::text {

class StyledRunTest : public ::testing::Test, protected TextFixture {
 protected:
  static auto run(std::uint32_t start, std::uint32_t end) -> TextRun {
    return TextRun{.start = start, .end = end};
  }

  static auto width(std::string_view text, const TextStyle& style) -> float {
    return layout(text, style).size().width;
  }

  static auto face_at(const TextLayout& laid, std::uint32_t offset)
      -> const FontFace* {
    for (const GlyphRun& glyph_run : laid.runs()) {
      if (offset >= glyph_run.text_start && offset < glyph_run.text_end) {
        return glyph_run.face.get();
      }
    }
    return nullptr;
  }
};

TEST_F(StyledRunTest, RunsAreSortedClippedAndFound) {
  TextRun bold = run(4, 8);
  bold.weight = font_weight::bold;
  TextRun italic = run(0, 6);
  italic.italic = true;
  const TextRuns runs{std::vector<TextRun>{bold, italic, run(9, 9)}};
  ASSERT_EQ(runs.runs().size(), 2U);
  EXPECT_EQ(runs.runs()[0].start, 0U);
  EXPECT_EQ(runs.runs()[1].start, 6U);
  EXPECT_EQ(runs.runs()[1].end, 8U);
  ASSERT_TRUE(runs.find(7).has_value());
  EXPECT_EQ(runs.find(7)->get().weight, font_weight::bold);
  EXPECT_FALSE(runs.find(8).has_value());
  EXPECT_EQ(runs, (TextRuns{std::vector<TextRun>{italic, bold}}));
  EXPECT_NE(runs.hash(), TextRuns{}.hash());
}

TEST_F(StyledRunTest, SplicedRunsFollowAnEllipsis) {
  TextRun first = run(0, 5);
  first.weight = font_weight::bold;
  TextRun last = run(10, 15);
  last.italic = true;
  const TextRuns runs{std::vector<TextRun>{first, last}};
  const TextRuns tail = runs.spliced(4, 3, 15, 15);
  ASSERT_EQ(tail.runs().size(), 1U);
  EXPECT_EQ(tail.runs()[0].end, 7U);
  const TextRuns head = runs.spliced(0, 3, 12, 15);
  ASSERT_EQ(head.runs().size(), 1U);
  EXPECT_EQ(head.runs()[0].start, 0U);
  EXPECT_EQ(head.runs()[0].end, 6U);
  EXPECT_TRUE(head.runs()[0].italic);
}

TEST_F(StyledRunTest, StyledRunsPickTheirOwnFaces) {
  const std::string text = "plain bold italic code";
  TextStyle style = TextFixture::style("en");
  TextRun bold = run(6, 10);
  bold.weight = font_weight::bold;
  TextRun italic = run(11, 17);
  italic.italic = true;
  TextRun code = run(18, 22);
  code.typeface = Typeface::monospaced;
  style.runs = TextRuns{std::vector<TextRun>{bold, italic, code}};
  const TextLayout laid = layout(text, style);
  const FontFace* plain = face_at(laid, 0);
  const FontFace* heavy = face_at(laid, 7);
  const FontFace* slanted = face_at(laid, 12);
  const FontFace* fixed = face_at(laid, 19);
  ASSERT_NE(plain, nullptr);
  ASSERT_NE(heavy, nullptr);
  ASSERT_NE(slanted, nullptr);
  ASSERT_NE(fixed, nullptr);
  EXPECT_EQ(plain->weight(), font_weight::regular);
  EXPECT_EQ(heavy->weight(), font_weight::bold);
  EXPECT_EQ(slanted->family(), "Inter");
  EXPECT_FALSE(slanted->shares_data_with(*plain));
  EXPECT_EQ(fixed->family(), "JetBrains Mono");
  EXPECT_EQ(laid.lines().size(), 1U);
}

TEST_F(StyledRunTest, AStyledRunWrapsAcrossLines) {
  const std::string text = "one two three four five six seven eight nine ten";
  TextStyle style = TextFixture::style("en");
  TextRun bold = run(8, 30);
  bold.weight = font_weight::bold;
  style.runs = TextRuns{std::vector<TextRun>{bold}};
  const TextLayout narrow = layout(text, style, 120.0F);
  EXPECT_GT(narrow.lines().size(), 2U);
  const TextLayout plain = layout(text, TextFixture::style("en"), 120.0F);
  EXPECT_GE(narrow.size().height, plain.size().height);
}

TEST_F(StyledRunTest, MonospacedLettersShareOneAdvance) {
  TextStyle style = TextFixture::style("en", 13.0F);
  style.typeface = Typeface::monospaced;
  EXPECT_NEAR(width("iii", style), width("WWW", style), 0.01F);
  EXPECT_NEAR(width("iii", style), width("111", style), 0.01F);
  const TextStyle standard = TextFixture::style("en", 13.0F);
  EXPECT_LT(width("iii", standard), width("WWW", standard));
}

TEST_F(StyledRunTest, DesignsResolveToTheirFamilies) {
  for (const auto& [typeface, family] :
       {std::pair{Typeface::serif, std::string_view{"Noto Serif"}},
        std::pair{Typeface::rounded, std::string_view{"Nunito"}},
        std::pair{Typeface::monospaced, std::string_view{"JetBrains Mono"}}}) {
    TextStyle style = TextFixture::style("en");
    style.typeface = typeface;
    const TextLayout laid = layout("Design", style);
    ASSERT_FALSE(laid.runs().empty());
    EXPECT_EQ(laid.runs().front().face->family(), family);
  }
}

TEST_F(StyledRunTest, WidthsScaleAdvancesLikeSwiftUI) {
  TextStyle style = TextFixture::style("en", 13.0F);
  const float standard = width("Width Sample", style);
  style.stretch = 0.81F;
  EXPECT_NEAR(width("Width Sample", style) / standard, 0.81F, 0.01F);
  style.stretch = 1.25F;
  EXPECT_NEAR(width("Width Sample", style) / standard, 1.25F, 0.01F);
}

TEST_F(StyledRunTest, ItalicWithoutAnItalicFaceIsSlanted) {
  TextStyle style = TextFixture::style("en");
  style.italic = true;
  style.typeface = Typeface::serif;
  const TextLayout laid = layout("Oblique", style);
  ASSERT_FALSE(laid.runs().empty());
  EXPECT_NEAR(laid.runs().front().face->options().slant,
              FontCollection::synthetic_slant, 0.0001F);
  style.typeface = Typeface::standard;
  const TextLayout inter = layout("Oblique", style);
  EXPECT_EQ(inter.runs().front().face->options().slant, 0.0F);
}

TEST_F(StyledRunTest, StyledRunsHoldAcrossScripts) {
  const std::string text =
      "Hi مرحبا 日本 "
      "สวัสดี";
  TextStyle style = TextFixture::style("en");
  TextRun bold = run(0, static_cast<std::uint32_t>(text.size()));
  bold.weight = font_weight::bold;
  bold.underline = true;
  style.runs = TextRuns{std::vector<TextRun>{bold}};
  const TextLayout styled = layout(text, style);
  const TextLayout plain = layout(text, TextFixture::style("en"));
  EXPECT_EQ(styled.glyphs().size(), plain.glyphs().size());
  std::set<std::string_view> families;
  for (const GlyphRun& glyph_run : styled.runs()) {
    families.insert(glyph_run.face->family());
  }
  EXPECT_GE(families.size(), 3U);
  EXPECT_EQ(face_at(styled, 0)->weight(), font_weight::bold);
}

TEST_F(StyledRunTest, TruncationCarriesRunsPastTheEllipsis) {
  auto typesetter = Typesetter::create(fonts());
  ASSERT_TRUE(typesetter.has_value());
  const std::string text = "Start middle finish of a long styled line";
  TextStyle style = TextFixture::style("en");
  TextRun last = run(33, static_cast<std::uint32_t>(text.size()));
  last.italic = true;
  style.runs = TextRuns{std::vector<TextRun>{last}};
  auto fitted =
      typesetter->fitted(text, style, LayoutWidth::points(150.0F).value(),
                         provider::SurfaceScale::unit(),
                         TextFitting{.lines = 1,
                                     .height = std::nullopt,
                                     .truncation = TruncationMode::head});
  ASSERT_TRUE(fitted.has_value());
  const FittedText& made = **fitted;
  EXPECT_TRUE(made.text.starts_with("…"));
  ASSERT_FALSE(made.style.runs.empty());
  EXPECT_EQ(made.style.runs.runs().back().end, made.text.size());
  EXPECT_TRUE(made.style.runs.runs().back().italic);
}

TEST_F(StyledRunTest, NamedFacesLeadTheirOwnChainOnly) {
  auto bytes = std::make_shared<const provider::AssetBlob>(
      read(fixture_font("NotoSansHebrew-Subset.ttf")));
  auto face = FontFace::create(bytes);
  ASSERT_TRUE(face.has_value());
  auto shared = std::make_shared<const FontFace>(std::move(*face));
  provider::headless::MemoryFonts system;
  const provider::standard::DirectoryAssets assets{
      std::filesystem::path{ARISA_SOURCE_ASSETS}};
  std::vector<FontEntry> app{FontEntry{.role = FontRole::latin,
                                       .face = shared,
                                       .design = FontDesign::text,
                                       .typeface = Typeface::standard,
                                       .italic = false,
                                       .name = "Hebrew"}};
  auto collection = FontCollection::load(assets, system, std::move(app));
  ASSERT_TRUE(collection.has_value());
  const Locale locale = Locale::undetermined();
  const auto named =
      collection->chain(locale, FaceQuery{.name = FontName{"Hebrew"}});
  ASSERT_FALSE(named->empty());
  EXPECT_EQ(named->front(), shared);
  const auto plain = collection->chain(locale, FaceQuery{});
  EXPECT_TRUE(std::ranges::none_of(
      *plain, [&](const auto& kept) { return kept == shared; }));
  const auto missing =
      collection->chain(locale, FaceQuery{.name = FontName{"Missing"}});
  EXPECT_EQ(missing->front(), plain->front());
}

TEST_F(StyledRunTest, DecorationMetricsComeFromTheFace) {
  const TextLayout laid = layout("Hxgy", TextFixture::style("en", 20.0F));
  const FaceMetrics metrics =
      laid.runs().front().face->metrics(PointSize::of<20.0F>());
  EXPECT_GT(metrics.underline_position, 0.0F);
  EXPECT_LT(metrics.underline_position, metrics.descent);
  EXPECT_GT(metrics.underline_thickness, 0.5F);
  EXPECT_GT(metrics.strikeout_position, 3.0F);
  EXPECT_LT(metrics.strikeout_position, metrics.ascent);
}

}
