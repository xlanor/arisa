#include <arisa/provider/fonts/font_bank.hpp>

#include <gtest/gtest.h>

#include <set>
#include <string_view>

namespace arisa::provider {

TEST(FontBankTest, DefaultsToArisaFonts) {
  const FontBank bank;
  EXPECT_EQ(bank.path(SystemFont::standard).value(),
            "arisa/fonts/standard.ttf");
  EXPECT_EQ(bank.path(SystemFont::nintendo_symbols).value(),
            "arisa/fonts/nintendo_symbols.ttf");
  std::set<std::string_view> names;
  for (const SystemFont font : all_system_fonts) {
    EXPECT_NE(FontBank::font_name(font), "unknown");
    names.insert(FontBank::font_name(font));
  }
  EXPECT_EQ(names.size(), all_system_fonts.size());
}

TEST(FontBankTest, WithReplacesExactlyOneFace) {
  const FontBank bank;
  const auto custom = AssetPath::create("ui/inter.otf");
  ASSERT_TRUE(custom.has_value());
  const FontBank updated = bank.with(SystemFont::korean, *custom);
  EXPECT_EQ(updated.path(SystemFont::korean), *custom);
  for (const SystemFont font : all_system_fonts) {
    if (font != SystemFont::korean) {
      EXPECT_EQ(updated.path(font), bank.path(font));
    }
  }
  EXPECT_NE(updated, bank);
  EXPECT_EQ(bank, FontBank{});
}

}
