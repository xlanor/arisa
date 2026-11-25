#include <arisa/text/case_mapping.hpp>
#include <arisa/text/date_format_style.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/locale_data.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/text/number_format_style.hpp>
#include <arisa/text/plural_rules.hpp>
#include <arisa/text/string_catalog.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::text {

class LocalizationTest : public ::testing::Test {
 protected:
  static auto locale(std::string_view tag) -> Locale {
    return Locale::create(tag).value_or(Locale::undetermined());
  }

  static constexpr std::chrono::sys_seconds sample{
      std::chrono::seconds{1'795'352'700}};

  static auto reference() -> const std::map<std::string, std::string>& {
    static const std::map<std::string, std::string> rows = [] {
      std::map<std::string, std::string> found;
      std::ifstream stream{ARISA_SWIFTUI_LOCALIZATION};
      std::string line;
      while (std::getline(stream, line)) {
        if (line.empty() || line.front() == '#') {
          continue;
        }
        std::istringstream fields{line};
        std::string key;
        std::string value;
        std::getline(fields, key, '\t');
        std::getline(fields, value, '\t');
        found[key] = value;
      }
      return found;
    }();
    return rows;
  }

  static auto measured(const std::string& key) -> std::string {
    const auto found = reference().find(key);
    if (found == reference().end()) {
      ADD_FAILURE() << "no SwiftUI row " << key;
      return {};
    }
    return found->second;
  }
};

TEST_F(LocalizationTest, BundlesEighteenLocalesAndFallsBackToEnglish) {
  EXPECT_EQ(LocaleData::bundled().size(), 18U);
  EXPECT_EQ(LocaleData::of(locale("ja-JP")).identifier(), "ja");
  EXPECT_EQ(LocaleData::of(locale("zh-TW")).identifier(), "zh-Hant");
  EXPECT_EQ(LocaleData::of(locale("zh-Hans-CN")).identifier(), "zh-Hans");
  EXPECT_EQ(LocaleData::of(locale("pt-PT")).identifier(), "pt-PT");
  EXPECT_EQ(LocaleData::of(locale("pt-BR")).identifier(), "pt");
  EXPECT_EQ(LocaleData::of(locale("es-MX")).identifier(), "es-419");
  EXPECT_EQ(LocaleData::of(locale("en-AU")).identifier(), "en-GB");
  EXPECT_EQ(LocaleData::of(locale("sv-SE")).identifier(), "en");
  EXPECT_EQ(LocaleData::of(Locale::undetermined()).identifier(), "en");
}

TEST_F(LocalizationTest, NumbersMatchSwiftUIOnMacOS) {
  const NumberFormatStyle number = NumberFormatStyle::number();
  const NumberFormatStyle percent = NumberFormatStyle::percent();
  for (const std::string tag :
       {"en_US", "de_DE", "fr_FR", "ja_JP", "es_ES", "ar", "he_IL", "ru_RU"}) {
    EXPECT_EQ(number.format(1234567.891, locale(tag)),
              measured("localization.number." + tag))
        << tag;
    EXPECT_EQ(number.format(1234.5, locale(tag)),
              measured("localization.number_small." + tag))
        << tag;
    EXPECT_EQ(percent.format(0.256, locale(tag)),
              measured("localization.percent." + tag))
        << tag;
  }
  EXPECT_EQ(number.format(0.1234567891234, locale("en_US")), "0.123457");
  EXPECT_EQ(percent.format(0.123456789, locale("en_US")), "12.345679%");
  EXPECT_EQ(number.format(12345.5, locale("es_ES")), "12.345,5");
  EXPECT_EQ(number.format(std::int64_t{-42}, locale("en")), "-42");
  EXPECT_EQ(number.fraction_length(2).format(3.0, locale("en")), "3.00");
  EXPECT_EQ(number.parse("1.234,5", locale("de_DE")), 1234.5);
}

TEST_F(LocalizationTest, DatesMatchSwiftUIOnMacOS) {
  using Style = DateFormatStyle;
  const std::chrono::seconds gmt{0};
  const std::array styles{
      std::pair{"numeric",
                Style{Style::DateStyle::numeric, Style::TimeStyle::omitted}},
      std::pair{"abbreviated", Style{Style::DateStyle::abbreviated,
                                     Style::TimeStyle::omitted}},
      std::pair{"long",
                Style{Style::DateStyle::long_form, Style::TimeStyle::omitted}},
      std::pair{"complete",
                Style{Style::DateStyle::complete, Style::TimeStyle::omitted}},
      std::pair{"time",
                Style{Style::DateStyle::omitted, Style::TimeStyle::shortened}},
      std::pair{"both", Style{Style::DateStyle::abbreviated,
                              Style::TimeStyle::shortened}}};
  constexpr std::array<std::string_view, 5> cldr_newer_than_macos{
      "complete.ja_JP", "abbreviated.ar", "long.ar", "complete.ar", "time.ar"};
  for (const std::string tag :
       {"en_US", "en_GB", "de_DE", "fr_FR", "ja_JP", "ar", "he_IL"}) {
    for (const auto& [name, style] : styles) {
      const std::string row = std::format("{}.{}", name, tag);
      if (std::ranges::contains(cldr_newer_than_macos, row) ||
          std::string_view{name} == "both") {
        continue;
      }
      EXPECT_EQ(style.format(sample, gmt, locale(tag)),
                measured(std::format("localization.date.{}.{}", name, tag)))
          << name << " " << tag;
    }
  }
  EXPECT_EQ(std::get<1>(styles[3]).format(sample, gmt, locale("ja_JP")),
            "2026年11月22日日曜日");
  EXPECT_EQ(std::get<1>(styles[1]).format(sample, gmt, locale("ar")),
            "22 نوفمبر 2026");
  EXPECT_EQ(std::get<1>(styles[4]).format(sample, gmt, locale("ar")), "1:05 م");
  const Style both{Style::DateStyle::abbreviated, Style::TimeStyle::shortened};
  EXPECT_EQ(both.format(sample, gmt, locale("en_US")),
            "Nov 22, 2026, 1:05\u202fPM");
  EXPECT_EQ(both.format(sample, gmt, locale("de_DE")),
            measured("localization.date.both.de_DE"));
  EXPECT_EQ(both.format(sample, gmt, locale("ja_JP")),
            measured("localization.date.both.ja_JP"));
  const Style numeric_time{Style::DateStyle::numeric,
                           Style::TimeStyle::shortened};
  EXPECT_EQ(numeric_time.format(sample, gmt, locale("en_US")),
            "11/22/2026, 1:05\u202fPM");
  EXPECT_EQ(Style(Style::DateStyle::omitted, Style::TimeStyle::shortened)
                .format(sample, std::chrono::hours{9}, locale("ja_JP")),
            "22:05");
}

TEST_F(LocalizationTest, WeekStartsFollowTheRegion) {
  EXPECT_EQ(LocaleData::of(locale("en_US")).first_weekday(),
            std::chrono::Sunday);
  EXPECT_EQ(LocaleData::of(locale("de")).first_weekday(), std::chrono::Monday);
  EXPECT_EQ(LocaleData::of(locale("ar")).first_weekday(),
            std::chrono::Saturday);
  EXPECT_EQ(LocaleData::of(locale("en")).first_weekday("GB"),
            std::chrono::Monday);
}

TEST_F(LocalizationTest, PluralRulesComeFromCldr) {
  const PluralRules english{locale("en")};
  EXPECT_EQ(english.category(std::int64_t{1}), PluralCategory::one);
  EXPECT_EQ(english.category(std::int64_t{2}), PluralCategory::other);
  EXPECT_EQ(english.category(1.0, 1), PluralCategory::other);
  const PluralRules arabic{locale("ar")};
  EXPECT_EQ(arabic.category(std::int64_t{0}), PluralCategory::zero);
  EXPECT_EQ(arabic.category(std::int64_t{1}), PluralCategory::one);
  EXPECT_EQ(arabic.category(std::int64_t{2}), PluralCategory::two);
  EXPECT_EQ(arabic.category(std::int64_t{5}), PluralCategory::few);
  EXPECT_EQ(arabic.category(std::int64_t{11}), PluralCategory::many);
  EXPECT_EQ(arabic.category(std::int64_t{100}), PluralCategory::other);
  const PluralRules russian{locale("ru")};
  EXPECT_EQ(russian.category(std::int64_t{21}), PluralCategory::one);
  EXPECT_EQ(russian.category(std::int64_t{22}), PluralCategory::few);
  EXPECT_EQ(russian.category(std::int64_t{25}), PluralCategory::many);
  const PluralRules japanese{locale("ja")};
  EXPECT_EQ(japanese.category(std::int64_t{1}), PluralCategory::other);
}

TEST_F(LocalizationTest, RelativeTimesMatchSwiftUIOnMacOS) {
  for (const std::string tag : {"en_US", "de_DE", "ja_JP", "ar"}) {
    EXPECT_EQ(
        RelativeDateTimeStyle::format(std::chrono::hours{-3}, locale(tag)),
        measured("localization.relative.past_hours." + tag))
        << tag;
    EXPECT_EQ(
        RelativeDateTimeStyle::format(std::chrono::hours{48}, locale(tag)),
        measured("localization.relative.future_days." + tag))
        << tag;
    EXPECT_EQ(
        RelativeDateTimeStyle::format(std::chrono::minutes{-1}, locale(tag)),
        measured("localization.relative.past_minute." + tag))
        << tag;
  }
}

TEST_F(LocalizationTest, CaseMappingFollowsTheLocale) {
  EXPECT_EQ(CaseMapping::uppercased("istanbul", locale("tr")),
            measured("localization.upper.tr"));
  EXPECT_EQ(CaseMapping::lowercased("ISTANBUL", locale("tr")),
            measured("localization.lower.tr"));
  EXPECT_EQ(CaseMapping::uppercased("άλφα", locale("el")),
            measured("localization.upper.el"));
  EXPECT_EQ(CaseMapping::lowercased("ΟΔΟΣ", locale("el")),
            measured("localization.lower.sigma"));
  EXPECT_EQ(CaseMapping::uppercased("istanbul", locale("tr")), "İSTANBUL");
  EXPECT_EQ(CaseMapping::lowercased("ISTANBUL", locale("tr")), "ıstanbul");
  EXPECT_EQ(CaseMapping::uppercased("istanbul", locale("en")), "ISTANBUL");
  EXPECT_EQ(CaseMapping::uppercased("άλφα", locale("el")), "ΑΛΦΑ");
  EXPECT_EQ(CaseMapping::lowercased("ΟΔΟΣ", locale("el")), "οδος");
  EXPECT_EQ(CaseMapping::lowercased("Ì", locale("lt")), "i̇̀");
}

TEST_F(LocalizationTest, LithuanianCasingFollowsTheSoftDottedContext) {
  EXPECT_EQ(CaseMapping::lowercased("I\u0328\u0301", locale("lt")),
            "i\u0307\u0328\u0301");
  EXPECT_EQ(CaseMapping::lowercased("J\u0303", locale("lt")), "j\u0307\u0303");
  EXPECT_EQ(CaseMapping::lowercased("I\u0328", locale("lt")), "i\u0328");
  EXPECT_EQ(CaseMapping::lowercased("I\u0328\u0301", locale("en")),
            "i\u0328\u0301");
  EXPECT_EQ(CaseMapping::uppercased("i\u0328\u0307\u0301", locale("lt")),
            "I\u0328\u0301");
  EXPECT_EQ(CaseMapping::uppercased("i\u0307", locale("lt")), "I");
  EXPECT_EQ(CaseMapping::uppercased("a\u0307", locale("lt")), "A\u0307");
}

TEST_F(LocalizationTest, CataloguesResolvePluralsAndFallBack) {
  const auto english = StringCatalog::parse(
      "\"Done\" = \"Done\"\n"
      "\"{0} files\" = { one = \"{0} file\", other = \"{0} files\" }\n",
      "en.toml");
  ASSERT_TRUE(english.has_value());
  const auto arabic = StringCatalog::parse(
      "\"Done\" = \"تم\"\n"
      "\"{0} files\" = { zero = \"z\", one = \"o\", two = \"t\", few = \"{0} "
      "f\", many = \"{0} m\", other = \"{0}\" }\n"
      "[Settings]\n\"Title\" = \"S\"\n",
      "ar.toml");
  ASSERT_TRUE(arabic.has_value());
  StringCatalog catalog;
  for (const auto& [table, entries] : *english) {
    catalog = catalog.with(locale("en"), table, entries);
  }
  for (const auto& [table, entries] : *arabic) {
    catalog = catalog.with(locale("ar"), table, entries);
  }
  EXPECT_EQ(catalog.resolve(LocalizedStringKey{"{0} files", 1}, locale("en")),
            "1 file");
  EXPECT_EQ(catalog.resolve(LocalizedStringKey{"{0} files", 3}, locale("en")),
            "3 files");
  EXPECT_EQ(catalog.resolve(LocalizedStringKey{"{0} files", 0}, locale("ar")),
            "z");
  EXPECT_EQ(catalog.resolve(LocalizedStringKey{"{0} files", 5}, locale("ar")),
            "5 f");
  EXPECT_EQ(catalog.resolve(LocalizedStringKey{"{0} files", 12}, locale("ar")),
            "12 m");
  EXPECT_EQ(catalog.resolve(LocalizedStringKey{"Done"}, locale("ar-EG")), "تم");
  EXPECT_EQ(catalog.resolve(LocalizedStringKey{"Title"}.table("Settings"),
                            locale("ar")),
            "S");
  EXPECT_EQ(
      catalog.resolve(LocalizedStringKey{"Missing {0}", "key"}, locale("ar")),
      "Missing key");
  EXPECT_EQ(catalog.resolve(LocalizedStringKey{"Done"}, locale("ja")), "Done");
}

TEST_F(LocalizationTest, PatternsPickThePluralFormForAGivenCount) {
  const auto english = StringCatalog::parse(
      "\"{0} weeks ago\" = { one = \"{0} week ago\", other = \"{0} weeks "
      "ago\" }\n\"Done\" = \"Finished\"\n",
      "en.toml");
  ASSERT_TRUE(english.has_value());
  const auto russian = StringCatalog::parse(
      "\"{0} weeks ago\" = { one = \"o\", few = \"f\", many = \"m\", "
      "other = \"x\" }\n",
      "ru.toml");
  ASSERT_TRUE(russian.has_value());
  StringCatalog catalog;
  for (const auto& [table, entries] : *english) {
    catalog = catalog.with(locale("en"), table, entries);
  }
  for (const auto& [table, entries] : *russian) {
    catalog = catalog.with(locale("ru"), table, entries);
  }
  const LocalizedStringKey weeks{"{0} weeks ago"};
  EXPECT_EQ(catalog.pattern(weeks, locale("en")), "{0} weeks ago");
  EXPECT_EQ(catalog.pattern(weeks, locale("en"), std::int64_t{1}),
            "{0} week ago");
  EXPECT_EQ(catalog.pattern(weeks, locale("en"), std::int64_t{2}),
            "{0} weeks ago");
  EXPECT_EQ(catalog.pattern(weeks, locale("en"), 1.5), "{0} weeks ago");
  EXPECT_EQ(catalog.pattern(weeks, locale("ru"), std::int64_t{21}), "o");
  EXPECT_EQ(catalog.pattern(weeks, locale("ru"), std::int64_t{3}), "f");
  EXPECT_EQ(catalog.pattern(weeks, locale("ru"), std::int64_t{5}), "m");
  EXPECT_EQ(catalog.pattern(LocalizedStringKey{"Done"}, locale("en"),
                            std::int64_t{1}),
            "Finished");
  EXPECT_EQ(catalog.pattern(LocalizedStringKey{"Missing"}, locale("en"),
                            std::int64_t{1}),
            "Missing");
}

TEST_F(LocalizationTest, BrokenCataloguesReportTheLineAndReason) {
  const auto broken =
      StringCatalog::parse("\"a\" = { one = \"x\" }\n", "fr.toml");
  ASSERT_FALSE(broken.has_value());
  EXPECT_EQ(broken.error().file, "fr.toml");
  EXPECT_EQ(broken.error().line, 1U);
  EXPECT_EQ(broken.error().reason, "plural forms need an other form");
  const auto unparsable = StringCatalog::parse("= nope", "de.toml");
  ASSERT_FALSE(unparsable.has_value());
  EXPECT_FALSE(unparsable.error().reason.empty());
}

}
