#include <arisa/components/system_symbols.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/standard/directory_assets.hpp>
#include <arisa/text/font_collection.hpp>
#include <arisa/text/font_role.hpp>
#include <arisa/text/utf8.hpp>

#include <text_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>

namespace arisa::text {

TEST(IconFontTest, BundledFontsIncludeTheSymbolFace) {
  const auto symbols = TextFixture::face(FontRole::symbols);
  ASSERT_NE(symbols, nullptr);
  EXPECT_FALSE(symbols->covers(U'A'));
  EXPECT_FALSE(symbols->covers(char32_t{0xE0E0}));
}

TEST(IconFontTest, EverySystemSymbolHasAGlyph) {
  const auto symbols = TextFixture::face(FontRole::symbols);
  ASSERT_NE(symbols, nullptr);
  for (std::size_t index = 0; index < components::SystemSymbols::names.size();
       ++index) {
    const char32_t codepoint = components::SystemSymbols::first_codepoint +
                               static_cast<char32_t>(index);
    EXPECT_TRUE(symbols->covers(codepoint))
        << components::SystemSymbols::names[index];
  }
  EXPECT_FALSE(symbols->covers(
      components::SystemSymbols::first_codepoint +
      static_cast<char32_t>(components::SystemSymbols::names.size())));
}

TEST(IconFontTest, EverySystemSymbolHasAFilledGlyph) {
  provider::headless::HeadlessProviderFactory factory;
  const provider::standard::DirectoryAssets assets{
      std::filesystem::path{ARISA_SOURCE_ASSETS}};
  auto fonts = FontCollection::load(assets, factory.fonts());
  ASSERT_TRUE(fonts.has_value());
  const auto filled =
      std::ranges::find_if(fonts->entries(), [](const FontEntry& entry) {
        return entry.role == FontRole::symbols &&
               entry.face->covers(
                   components::SystemSymbols::filled_first_codepoint);
      });
  ASSERT_NE(filled, fonts->entries().end());
  for (std::size_t index = 0; index < components::SystemSymbols::names.size();
       ++index) {
    EXPECT_TRUE(
        filled->face->covers(components::SystemSymbols::filled_first_codepoint +
                             static_cast<char32_t>(index)))
        << components::SystemSymbols::names[index];
    EXPECT_FALSE(
        filled->face->covers(components::SystemSymbols::first_codepoint +
                             static_cast<char32_t>(index)));
  }
}

TEST(IconFontTest, SymbolsShapeWithTheSymbolFace) {
  const std::string text =
      Utf8::encode(components::SystemSymbols::first_codepoint + 1) + "ok";
  const TextLayout laid = TextFixture::layout(text, TextFixture::style("en"));
  ASSERT_GE(laid.runs().size(), 2U);
  EXPECT_NE(laid.runs().front().face, laid.runs().back().face);
  EXPECT_GT(laid.size().width, 20.0F);
}

TEST(IconFontTest, UtfEncodingRoundTripsEveryPlane) {
  for (const char32_t codepoint :
       {char32_t{0x41}, char32_t{0xE9}, char32_t{0x20AC}, char32_t{0x100000}}) {
    const std::string encoded = Utf8::encode(codepoint);
    EXPECT_TRUE(Utf8::valid(encoded));
    EXPECT_EQ(Utf8::decode(encoded, 0).codepoint, codepoint);
    EXPECT_EQ(Utf8::decode(encoded, 0).length, encoded.size());
  }
}

}
