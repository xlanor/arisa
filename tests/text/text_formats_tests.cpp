#include <arisa/text/byte_count_format_style.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/locale_data.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/text/measurement.hpp>
#include <arisa/text/number_format_style.hpp>
#include <arisa/text/string_catalog.hpp>

#include <gtest/gtest.h>

#include <charconv>
#include <cstdint>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace arisa::text {

class TextFormatsTest : public ::testing::Test {
 protected:
  using Key = std::tuple<std::string, std::string, std::string, std::string>;

  static auto reference() -> const std::map<Key, std::string>& {
    static const std::map<Key, std::string> rows = [] {
      std::map<Key, std::string> loaded;
      std::ifstream stream{ARISA_SWIFTUI_FORMATS};
      std::string line;
      std::getline(stream, line);
      while (std::getline(stream, line)) {
        std::vector<std::string> cells;
        std::size_t start = 0;
        for (std::size_t at = 0; at <= line.size(); ++at) {
          if (at == line.size() || line[at] == '\t') {
            cells.push_back(line.substr(start, at - start));
            start = at + 1;
          }
        }
        if (cells.size() >= 5) {
          loaded.emplace(Key{cells[0], cells[1], cells[2], cells[3]}, cells[4]);
        }
      }
      return loaded;
    }();
    return rows;
  }

  static auto expected(std::string_view locale, std::string_view kind,
                       std::string_view key, std::string_view sample)
      -> std::string {
    const auto found =
        reference().find(Key{std::string{locale}, std::string{kind},
                             std::string{key}, std::string{sample}});
    return found == reference().end() ? std::string{"<missing>"}
                                      : found->second;
  }

  static auto locale(std::string_view tag) -> Locale {
    return Locale::create(tag).value_or(Locale::undetermined());
  }

  static constexpr std::array<std::string_view, 18> locales{
      "en",      "en-GB",   "ja", "fr", "fr-CA", "de",    "it", "es", "es-419",
      "zh-Hans", "zh-Hant", "ko", "nl", "pt",    "pt-PT", "ru", "ar", "he"};
};

TEST_F(TextFormatsTest, CurrencyMatchesFoundationInEveryBundledLocale) {
  ASSERT_FALSE(reference().empty());
  for (const std::string_view tag : locales) {
    for (const std::string_view code : {"USD", "EUR", "JPY", "CHF", "GBP"}) {
      const NumberFormatStyle style = NumberFormatStyle::currency(code);
      EXPECT_EQ(style.format(1234.5, locale(tag)),
                expected(tag, "currency_sample", code, "1234.5"))
          << tag << " " << code;
      EXPECT_EQ(
          style.presentation(NumberFormatStyle::CurrencyPresentation::iso_code)
              .format(1234.5, locale(tag)),
          expected(tag, "currency_sample_iso", code, "1234.5"))
          << tag << " " << code;
    }
  }
}

TEST_F(TextFormatsTest, NegativeCurrencyAndDigitsFollowTheCode) {
  for (const std::string_view tag : {"en", "fr", "de", "ja", "nl", "ru"}) {
    EXPECT_EQ(NumberFormatStyle::currency("EUR").format(-0.5, locale(tag)),
              expected(tag, "currency_sample", "EUR", "-0.5"))
        << tag;
  }
  EXPECT_EQ(LocaleData::currency_fraction_digits("JPY"), 0U);
  EXPECT_EQ(LocaleData::currency_fraction_digits("USD"), 2U);
  EXPECT_EQ(NumberFormatStyle::currency("jpy").currency_code(), "JPY");
  EXPECT_EQ(
      NumberFormatStyle::currency("USD").format(std::int64_t{3}, locale("en")),
      "$3.00");
}

TEST_F(TextFormatsTest, ByteCountsMatchFoundation) {
  using Style = ByteCountFormatStyle::Style;
  const std::vector<std::pair<std::string_view, Style>> styles{
      {"file", Style::file},
      {"memory", Style::memory},
      {"decimal", Style::decimal},
      {"binary", Style::binary}};
  for (const std::string_view tag : {"en", "fr", "de", "ru", "ja"}) {
    for (const auto& [name, style] : styles) {
      for (const std::int64_t bytes :
           {0LL, 1LL, 2LL, 21LL, 950LL, 999LL, 1000LL, 1500LL, 12345LL,
            1500000LL, 1572864LL, 999999999LL, 3221225472LL, 2000000000000LL}) {
        EXPECT_EQ(ByteCountFormatStyle{style}.format(bytes, locale(tag)),
                  expected(tag, "bytes", name, std::to_string(bytes)))
            << tag << " " << name << " " << bytes;
      }
    }
  }
  EXPECT_EQ(ByteCountFormatStyle(Style::file, ByteCountFormatStyle::Units::all,
                                 true, true)
                .format(1500000, locale("en")),
            expected("en", "bytes_actual", "file", "1500000"));
  EXPECT_EQ(
      ByteCountFormatStyle(Style::file, ByteCountFormatStyle::Unit::kb, false)
          .format(2000, locale("en")),
      expected("en", "bytes_unit", "kb", "2"));
}

TEST_F(TextFormatsTest, MeasurementsMatchFoundationInEveryWidth) {
  using Unit = Measurement::Unit;
  using Width = MeasurementFormatStyle::UnitWidth;
  const std::vector<std::pair<std::string_view, Unit>> units{
      {"meter", Unit::meters},
      {"kilometer", Unit::kilometers},
      {"mile", Unit::miles},
      {"kilogram", Unit::kilograms},
      {"celsius", Unit::celsius},
      {"liter", Unit::liters},
      {"kilometer-per-hour", Unit::kilometers_per_hour},
      {"hour", Unit::hours}};
  const std::vector<std::pair<std::string_view, Width>> widths{
      {"wide", Width::wide},
      {"abbreviated", Width::abbreviated},
      {"narrow", Width::narrow}};
  for (const std::string_view tag : {"en", "fr", "de", "ru", "ja", "ar"}) {
    for (const auto& [name, unit] : units) {
      for (const auto& [width_name, width] : widths) {
        for (const double value : {1.0, 2.0, 5.0, 21.0, 1.5}) {
          const std::string sample =
              value == 1.5 ? "1.5" : std::to_string(static_cast<int>(value));
          EXPECT_EQ(MeasurementFormatStyle{width}.format(
                        Measurement{value, unit}, locale(tag)),
                    expected(tag, "measurement",
                             std::string{name} + "." + std::string{width_name},
                             sample))
              << tag << " " << name << " " << width_name << " " << value;
        }
      }
    }
  }
}

TEST_F(TextFormatsTest, MeasurementsConvertWithinADimension) {
  const auto miles = Measurement{1.0, Measurement::Unit::miles}.converted(
      Measurement::Unit::kilometers);
  ASSERT_TRUE(miles.has_value());
  EXPECT_NEAR(miles->value(), 1.609344, 1e-9);
  const auto boiling = Measurement{100.0, Measurement::Unit::celsius}.converted(
      Measurement::Unit::fahrenheit);
  ASSERT_TRUE(boiling.has_value());
  EXPECT_NEAR(boiling->value(), 212.0, 1e-6);
  EXPECT_FALSE((Measurement{1.0, Measurement::Unit::meters}
                    .converted(Measurement::Unit::grams)
                    .has_value()));
}

TEST_F(TextFormatsTest, KeysInterpolateFormattedValuesAndSpecifiers) {
  const StringCatalog catalog;
  const LocalizedStringKey priced{
      "Total {} for {}",
      LocalizedStringKey::formatted(9.99, NumberFormatStyle::currency("USD")),
      LocalizedStringKey::specified(3.14159, "%.2f")};
  EXPECT_EQ(catalog.resolve(priced, locale("en")), "Total $9.99 for 3.14");
  const LocalizedStringKey sized{
      "Size {}", LocalizedStringKey::formatter(
                     std::int64_t{1500000},
                     ByteCountFormatStyle{ByteCountFormatStyle::Style::file})};
  EXPECT_EQ(catalog.resolve(sized, locale("en")), "Size 1.5 MB");
  EXPECT_EQ(FormatSpecifier{"%05.1f"}.format(3.14159, locale("en")), "003.1");
  EXPECT_EQ(FormatSpecifier{"%x"}.format(std::int64_t{255}, locale("en")),
            "ff");
  EXPECT_EQ(FormatSpecifier{"%d"}.format(2.9, locale("en")), "2");
  EXPECT_EQ(
      FormatSpecifier{"%.3@"}.format(std::string_view{"abcdef"}, locale("en")),
      "abc");
  EXPECT_EQ(priced, priced);
  const LocalizedStringKey other{
      "Total {} for {}",
      LocalizedStringKey::formatted(9.99, NumberFormatStyle::currency("EUR")),
      LocalizedStringKey::specified(3.14159, "%.2f")};
  EXPECT_NE(priced, other);
}

TEST_F(TextFormatsTest, SegmentsSplitALocalizedPattern) {
  const std::vector<StringCatalog::Segment> segments =
      StringCatalog::segments("a {} b {0} {9}", 2);
  ASSERT_EQ(segments.size(), 5U);
  EXPECT_EQ(segments[0].literal, "a ");
  EXPECT_EQ(segments[1].argument, std::optional<std::size_t>{0});
  EXPECT_EQ(segments[3].argument, std::optional<std::size_t>{0});
  EXPECT_EQ(segments[4].literal, " {9}");
}

}
