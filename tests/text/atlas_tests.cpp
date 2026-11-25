#include <text_fixture.hpp>

#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/text/glyph_atlas.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arisa::text {

class TextAtlas : public ::testing::Test {
 protected:
  static auto latin() -> const FontFace& {
    return *TextFixture::face(FontRole::latin);
  }

  static auto glyph(char32_t codepoint) -> GlyphId {
    return latin().glyph(codepoint).value();
  }

  static auto place(GlyphAtlas& atlas, char32_t codepoint, float size,
                    float scale = 1.0F, float x = 0.0F)
      -> std::expected<std::optional<GlyphQuad>, TextError> {
    return atlas.place(latin(), glyph(codepoint),
                       PointSize::create(size).value(),
                       provider::SurfaceScale::create(scale).value(), x, 20.0F);
  }

  provider::headless::NullGpu gpu_;
};

TEST_F(TextAtlas, UploadsOnlyDirtyRegions) {
  auto atlas = GlyphAtlas::create(gpu_);
  ASSERT_TRUE(atlas.has_value());
  const TextLayout layout =
      TextFixture::layout("Hello, world", TextFixture::style("en"));
  for (const GlyphRun& run : layout.runs()) {
    for (const PositionedGlyph& positioned : layout.glyphs(run)) {
      ASSERT_TRUE(atlas
                      ->place(*run.face, positioned.glyph, run.size,
                              layout.scale(), positioned.x, positioned.y)
                      .has_value());
    }
  }
  EXPECT_EQ(gpu_.uploads(), 0U);
  ASSERT_TRUE(atlas->flush().has_value());
  const std::size_t page_bytes = std::size_t{1024} * 1024;
  const std::size_t uploaded = gpu_.uploaded_bytes();
  EXPECT_GT(uploaded, 0U);
  EXPECT_LT(uploaded, page_bytes / 64);
  EXPECT_EQ(gpu_.uploads(), atlas->statistics().uploads);
  const std::size_t rasterised = atlas->statistics().rasterised;
  for (const GlyphRun& run : layout.runs()) {
    for (const PositionedGlyph& positioned : layout.glyphs(run)) {
      ASSERT_TRUE(atlas
                      ->place(*run.face, positioned.glyph, run.size,
                              layout.scale(), positioned.x, positioned.y)
                      .has_value());
    }
  }
  ASSERT_TRUE(atlas->flush().has_value());
  EXPECT_EQ(gpu_.uploaded_bytes(), uploaded);
  EXPECT_EQ(atlas->statistics().rasterised, rasterised);
  ASSERT_TRUE(place(*atlas, U'Q', 20.0F).has_value());
  ASSERT_TRUE(atlas->flush().has_value());
  const std::size_t added = gpu_.uploaded_bytes() - uploaded;
  EXPECT_GT(added, 0U);
  EXPECT_LT(added, page_bytes / 256);
}

TEST_F(TextAtlas, SpacesTakeNoRoom) {
  auto atlas = GlyphAtlas::create(gpu_);
  ASSERT_TRUE(atlas.has_value());
  auto space = place(*atlas, U' ', 20.0F);
  ASSERT_TRUE(space.has_value());
  EXPECT_FALSE(space->has_value());
  ASSERT_TRUE(atlas->flush().has_value());
  EXPECT_EQ(gpu_.uploaded_bytes(), 0U);
}

TEST_F(TextAtlas, GrowsByAddingPages) {
  auto atlas = GlyphAtlas::create(
      gpu_, AtlasOptions{.page_size = 64, .maximum_pages = 8});
  ASSERT_TRUE(atlas.has_value());
  EXPECT_EQ(atlas->pages().size(), 1U);
  for (char32_t codepoint = U'A'; codepoint <= U'L'; ++codepoint) {
    ASSERT_TRUE(place(*atlas, codepoint, 30.0F).has_value());
  }
  EXPECT_GT(atlas->pages().size(), 1U);
  EXPECT_EQ(gpu_.live_textures(), atlas->pages().size());
  EXPECT_EQ(atlas->statistics().evictions, 0U);
}

TEST_F(TextAtlas, EvictsTheLeastRecentlyUsedPageWhenFull) {
  auto atlas = GlyphAtlas::create(
      gpu_, AtlasOptions{.page_size = 64, .maximum_pages = 1});
  ASSERT_TRUE(atlas.has_value());
  char32_t next = U'A';
  std::optional<TextError> full;
  for (; next <= U'Z' && !full.has_value(); ++next) {
    auto placed = place(*atlas, next, 30.0F);
    if (!placed.has_value()) {
      full = placed.error();
    }
  }
  ASSERT_TRUE(full.has_value());
  EXPECT_EQ(full->code(), TextErrorCode::atlas_full);
  EXPECT_EQ(atlas->statistics().evictions, 0U);
  atlas->advance_frame();
  EXPECT_EQ(atlas->evicted_pages(), 0U);
  ASSERT_TRUE(place(*atlas, U'a', 30.0F).has_value());
  EXPECT_EQ(atlas->statistics().evictions, 1U);
  EXPECT_EQ(atlas->evicted_pages(), 1U);
  EXPECT_EQ(atlas->pages().size(), 1U);
  EXPECT_EQ(atlas->statistics().glyphs, 1U);
  atlas->advance_frame();
  EXPECT_EQ(atlas->evicted_pages(), 0U);
}

TEST_F(TextAtlas, ScaleChangeRasterisesAgain) {
  auto atlas = GlyphAtlas::create(gpu_);
  ASSERT_TRUE(atlas.has_value());
  auto handheld = place(*atlas, U'H', 20.0F, 1.0F);
  auto docked = place(*atlas, U'H', 20.0F, 1.5F);
  ASSERT_TRUE(handheld.has_value() && docked.has_value());
  ASSERT_TRUE(handheld->has_value() && docked->has_value());
  EXPECT_EQ(atlas->statistics().rasterised, 2U);
  const float small = (*handheld)->bottom - (*handheld)->top;
  const float large = (*docked)->bottom - (*docked)->top;
  EXPECT_NEAR(large / small, 1.5F, 0.15F);
}

TEST_F(TextAtlas, SubpixelPositionsShareBins) {
  auto atlas = GlyphAtlas::create(gpu_);
  ASSERT_TRUE(atlas.has_value());
  ASSERT_TRUE(place(*atlas, U'o', 20.0F, 1.0F, 0.0F).has_value());
  ASSERT_TRUE(place(*atlas, U'o', 20.0F, 1.0F, 0.5F).has_value());
  EXPECT_EQ(atlas->statistics().rasterised, 2U);
  auto shifted = place(*atlas, U'o', 20.0F, 1.0F, 3.0F);
  auto origin = place(*atlas, U'o', 20.0F, 1.0F, 0.0F);
  EXPECT_EQ(atlas->statistics().rasterised, 2U);
  ASSERT_TRUE(shifted.has_value() && origin.has_value());
  EXPECT_FLOAT_EQ((*shifted)->left - (*origin)->left, 3.0F);
  EXPECT_FLOAT_EQ((*shifted)->u0, (*origin)->u0);
}

TEST_F(TextAtlas, UnquantisedGlyphsKeepTheirFractionalPosition) {
  auto atlas = GlyphAtlas::create(gpu_);
  ASSERT_TRUE(atlas.has_value());
  const PointSize size = PointSize::create(20.0F).value();
  const provider::SurfaceScale scale = provider::SurfaceScale::unit();
  auto snapped = atlas->place(latin(), glyph(U'A'), size, scale, 10.37F, 20.4F);
  auto exact =
      atlas->place(latin(), glyph(U'A'), size, scale, 10.37F, 20.4F, false);
  ASSERT_TRUE(snapped.has_value() && snapped->has_value());
  ASSERT_TRUE(exact.has_value() && exact->has_value());
  const GlyphQuad& rounded = **snapped;
  const GlyphQuad& precise = **exact;
  EXPECT_FLOAT_EQ(rounded.top, std::round(rounded.top));
  EXPECT_NEAR(precise.top - std::floor(precise.top), 0.4F, 0.001F);
  EXPECT_NEAR(precise.left - std::floor(precise.left), 0.37F, 0.001F);
  EXPECT_FLOAT_EQ(rounded.left, std::round(rounded.left));
}

TEST_F(TextAtlas, RejectsInvalidOptions) {
  auto atlas = GlyphAtlas::create(gpu_, AtlasOptions{.page_size = 16});
  ASSERT_FALSE(atlas.has_value());
  EXPECT_EQ(atlas.error().code(), TextErrorCode::invalid_argument);
}

}
