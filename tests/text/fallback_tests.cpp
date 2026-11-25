#include <text_fixture.hpp>

#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/text/glyph_atlas.hpp>
#include <arisa/text/software_renderer.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace arisa::text {

TEST(TextFallback, LocalePicksTheHanFaceForTheSameString) {
  const std::string text = "直角";
  const auto japanese = TextFixture::face(FontRole::japanese);
  const auto simplified = TextFixture::face(FontRole::chinese_simplified);
  const auto traditional = TextFixture::face(FontRole::chinese_traditional);
  ASSERT_NE(japanese, nullptr);
  ASSERT_NE(simplified, nullptr);
  ASSERT_NE(traditional, nullptr);
  const TextLayout ja = TextFixture::layout(text, TextFixture::style("ja"));
  const TextLayout hans =
      TextFixture::layout(text, TextFixture::style("zh-Hans"));
  const TextLayout hant =
      TextFixture::layout(text, TextFixture::style("zh-Hant"));
  ASSERT_EQ(ja.runs().size(), 1U);
  ASSERT_EQ(hans.runs().size(), 1U);
  ASSERT_EQ(hant.runs().size(), 1U);
  EXPECT_EQ(ja.runs().front().face, japanese);
  EXPECT_EQ(hans.runs().front().face, simplified);
  EXPECT_EQ(hant.runs().front().face, traditional);
}

TEST(TextFallback, ChainOrdersCjkByLocale) {
  const FontCollection& fonts = TextFixture::fonts();
  const auto position =
      [](const std::vector<std::shared_ptr<const FontFace>>& chain,
         const std::shared_ptr<const FontFace>& face) {
        return std::ranges::find(chain, face) - chain.begin();
      };
  const auto japanese = TextFixture::face(FontRole::japanese);
  const auto simplified = TextFixture::face(FontRole::chinese_simplified);
  const auto korean = TextFixture::face(FontRole::korean);
  const auto latin = TextFixture::face(FontRole::latin);
  const auto ja_chain = fonts.chain(*Locale::create("ja"), font_weight::regular,
                                    FontDesign::text);
  const auto hans_chain = fonts.chain(*Locale::create("zh-CN"),
                                      font_weight::regular, FontDesign::text);
  const auto ko_chain = fonts.chain(*Locale::create("ko"), font_weight::regular,
                                    FontDesign::text);
  const FontCollection::Chain& ja = *ja_chain;
  const FontCollection::Chain& hans = *hans_chain;
  const FontCollection::Chain& ko = *ko_chain;
  EXPECT_EQ(ja.front(), latin);
  EXPECT_LT(position(ja, japanese), position(ja, simplified));
  EXPECT_LT(position(hans, simplified), position(hans, japanese));
  EXPECT_LT(position(ko, korean), position(ko, japanese));
  const auto latin_faces = std::ranges::count_if(
      ja, [](const auto& face) { return face->variable(); });
  EXPECT_EQ(latin_faces, 1);
}

TEST(TextFallback, ChainIsBuiltOncePerLocaleWeightAndDesign) {
  const FontCollection& fonts = TextFixture::fonts();
  const auto first = fonts.chain(*Locale::create("en"), font_weight::regular,
                                 FontDesign::text);
  const auto again = fonts.chain(*Locale::create("en-GB"), font_weight::regular,
                                 FontDesign::text);
  const auto bold =
      fonts.chain(*Locale::create("en"), font_weight::bold, FontDesign::text);
  const auto korean = fonts.chain(*Locale::create("ko"), font_weight::regular,
                                  FontDesign::text);
  EXPECT_EQ(first.get(), again.get());
  EXPECT_NE(first.get(), bold.get());
  EXPECT_NE(first.get(), korean.get());
  EXPECT_FALSE(first->empty());
}

TEST(TextFallback, CoverageSplitsRunsAtFallbackBoundaries) {
  const std::string text = "Hello 日本 مرحبا";
  const TextLayout layout = TextFixture::layout(text, TextFixture::style("ja"));
  const auto japanese = TextFixture::face(FontRole::japanese);
  const auto arabic = TextFixture::script_face("NotoSansArabic-Subset.ttf");
  std::vector<std::shared_ptr<const FontFace>> faces;
  for (const GlyphRun& run : layout.runs()) {
    faces.push_back(run.face);
    for (const PositionedGlyph& glyph : layout.glyphs(run)) {
      EXPECT_NE(glyph.glyph.value(), 0U);
    }
  }
  ASSERT_EQ(faces.size(), 3U);
  EXPECT_TRUE(faces[0]->variable());
  EXPECT_EQ(faces[1], japanese);
  EXPECT_EQ(faces[2], arabic);
}

TEST(TextFallback, WeightPicksTheMatchingInterInstance) {
  TextStyle bold = TextFixture::style("en");
  bold.weight = font_weight::bold;
  const TextLayout layout = TextFixture::layout("Bold", bold);
  ASSERT_EQ(layout.runs().size(), 1U);
  EXPECT_EQ(layout.runs().front().face->weight(), font_weight::bold);
  const TextLayout regular =
      TextFixture::layout("Bold", TextFixture::style("en"));
  EXPECT_GT(layout.size().width, regular.size().width);
}

TEST(TextFallback, PrivateUseCodepointsRenderFromTheSymbolsFace) {
  provider::headless::MemoryFonts system;
  system.add(
      provider::SystemFont::nintendo_symbols,
      TextFixture::read(TextFixture::fixture_font("ArisaTestSymbols.ttf")));
  const provider::standard::DirectoryAssets assets{
      std::filesystem::path{ARISA_SOURCE_ASSETS}};
  auto fonts = FontCollection::load(assets, system);
  ASSERT_TRUE(fonts.has_value());
  const auto symbols =
      std::ranges::find_if(fonts->entries(), [](const FontEntry& entry) {
        return entry.role == FontRole::symbols && entry.face->covers(U'\uE0E0');
      });
  ASSERT_NE(symbols, fonts->entries().end());
  const auto chain = fonts->chain(*Locale::create("en"), font_weight::regular,
                                  FontDesign::text);
  ASSERT_TRUE(chain->front()->covers(U'\uE0E0'));
  ASSERT_TRUE(symbols->face->covers(U'\uE0E0'));
  const auto scale = provider::SurfaceScale::create(1.0F).value();
  auto mixed =
      TextLayout::create(*fonts, "\uE0E0 OK \uE0E3", TextFixture::style("en"),
                         LayoutWidth::unbounded(), scale);
  ASSERT_TRUE(mixed.has_value());
  ASSERT_GE(mixed->runs().size(), 3U);
  EXPECT_EQ(mixed->runs().front().face, symbols->face);
  EXPECT_EQ(mixed->runs().back().face, symbols->face);
  EXPECT_TRUE(std::ranges::any_of(mixed->runs(), [&](const GlyphRun& run) {
    return run.face == chain->front();
  }));
  EXPECT_EQ(mixed->glyphs(mixed->runs().front()).front().glyph,
            symbols->face->glyph(U'\uE0E0'));
  auto glyph = TextLayout::create(*fonts, "\uE0E0", TextFixture::style("en"),
                                  LayoutWidth::unbounded(), scale);
  ASSERT_TRUE(glyph.has_value());
  provider::headless::NullGpu gpu;
  auto atlas = GlyphAtlas::create(gpu);
  ASSERT_TRUE(atlas.has_value());
  auto image = SoftwareRenderer::render(
      *glyph, *atlas,
      provider::Colour{.red = 0.0F, .green = 0.0F, .blue = 0.0F},
      provider::Colour{.red = 1.0F, .green = 1.0F, .blue = 1.0F}, 4);
  ASSERT_TRUE(image.has_value());
  std::size_t solid = 0;
  for (std::uint32_t y = 0; y < image->height(); ++y) {
    for (std::uint32_t x = 0; x < image->width(); ++x) {
      if (image->pixel(x, y).value()[0] < 64) {
        ++solid;
      }
    }
  }
  EXPECT_GE(solid, 180U);
}

TEST(TextFallback, LoadSkipsUnreadableSystemFonts) {
  provider::headless::HeadlessProviderFactory factory;
  const provider::standard::DirectoryAssets assets{
      std::filesystem::path{ARISA_SOURCE_ASSETS}};
  auto fonts = FontCollection::load(assets, factory.fonts());
  ASSERT_TRUE(fonts.has_value());
  EXPECT_EQ(fonts->entries().size(), 20U);
  EXPECT_EQ(
      std::ranges::count(fonts->entries(), FontRole::latin, &FontEntry::role),
      18);
  EXPECT_EQ(
      std::ranges::count(fonts->entries(), FontRole::symbols, &FontEntry::role),
      2);
}

TEST(TextFallback, LoadFailsWithoutTheBundledFont) {
  provider::headless::HeadlessProviderFactory factory;
  auto fonts = FontCollection::load(factory.assets(), factory.fonts());
  ASSERT_FALSE(fonts.has_value());
  EXPECT_EQ(fonts.error().code(), TextErrorCode::provider_failure);
}

}
