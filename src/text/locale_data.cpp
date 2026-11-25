#include <arisa/text/locale_data.hpp>

#include <text/format_record.hpp>
#include <text/locale_record.hpp>

#include <algorithm>
#include <array>
#include <utility>

namespace arisa::text {

auto LocaleData::bundled() noexcept -> std::span<const Record> {
  return records();
}

auto LocaleData::at(std::size_t index) noexcept -> LocaleData {
  const std::span<const Record> all = records();
  return LocaleData{all[std::min(index, all.size() - 1)]};
}

auto LocaleData::find(std::string_view identifier) noexcept
    -> std::optional<LocaleData> {
  for (const Record& record : records()) {
    if (record.identifier == identifier) {
      return LocaleData{record};
    }
  }
  return std::nullopt;
}

auto LocaleData::identifier_for(const Locale& locale) noexcept
    -> std::string_view {
  const std::string_view language = locale.language();
  const std::string_view region = locale.region();
  const std::string_view script = locale.script();
  if (language == "en") {
    constexpr std::array<std::string_view, 12> british{
        "GB", "IE", "AU", "NZ", "IN", "SG", "ZA", "HK", "MT", "MY", "PK", "NG"};
    return std::ranges::contains(british, region) ? "en-GB" : "en";
  }
  if (language == "zh") {
    if (script == "Hant" ||
        (script.empty() &&
         (region == "TW" || region == "HK" || region == "MO"))) {
      return "zh-Hant";
    }
    return "zh-Hans";
  }
  if (language == "pt") {
    return !region.empty() && region != "BR" ? "pt-PT" : "pt";
  }
  if (language == "es") {
    return !region.empty() && region != "ES" && region != "GQ" ? "es-419"
                                                               : "es";
  }
  if (language == "fr") {
    return region == "CA" ? "fr-CA" : "fr";
  }
  if (language == "iw") {
    return "he";
  }
  return language;
}

auto LocaleData::of(const Locale& locale) noexcept -> LocaleData {
  if (const auto found = find(identifier_for(locale))) {
    return *found;
  }
  return LocaleData{records().front()};
}

auto LocaleData::identifier() const noexcept -> std::string_view {
  return record_->identifier;
}

auto LocaleData::region() const noexcept -> std::string_view {
  return record_->region;
}

auto LocaleData::decimal_separator() const noexcept -> std::string_view {
  return record_->decimal;
}

auto LocaleData::grouping_separator() const noexcept -> std::string_view {
  return record_->group;
}

auto LocaleData::minus_sign() const noexcept -> std::string_view {
  return record_->minus;
}

auto LocaleData::plus_sign() const noexcept -> std::string_view {
  return record_->plus;
}

auto LocaleData::percent_sign() const noexcept -> std::string_view {
  return record_->percent_sign;
}

auto LocaleData::decimal_pattern() const noexcept -> std::string_view {
  return record_->decimal_pattern;
}

auto LocaleData::percent_pattern() const noexcept -> std::string_view {
  return record_->percent_pattern;
}

auto LocaleData::minimum_grouping_digits() const noexcept -> unsigned {
  return record_->minimum_grouping;
}

auto LocaleData::month(std::chrono::month month, Width width,
                       Context context) const noexcept -> std::string_view {
  const unsigned index = month.ok() ? static_cast<unsigned>(month) - 1U : 0U;
  if (width == Width::wide) {
    return context == Context::standalone ? record_->months_standalone[index]
                                          : record_->months_wide[index];
  }
  return record_->months_abbreviated[index];
}

auto LocaleData::weekday(std::chrono::weekday weekday,
                         Width width) const noexcept -> std::string_view {
  const unsigned index = weekday.c_encoding() % 7U;
  switch (width) {
    case Width::wide:
      return record_->weekdays_wide[index];
    case Width::abbreviated:
      return record_->weekdays_abbreviated[index];
    case Width::short_form:
      return record_->weekdays_short[index];
    case Width::narrow:
      break;
  }
  return record_->weekdays_narrow[index];
}

auto LocaleData::day_period(bool after_noon) const noexcept
    -> std::string_view {
  return after_noon ? record_->pm : record_->am;
}

auto LocaleData::date_pattern(Length length) const noexcept
    -> std::string_view {
  return record_->date_patterns[std::to_underlying(length)];
}

auto LocaleData::time_pattern(Length length) const noexcept
    -> std::string_view {
  return record_->time_patterns[std::to_underlying(length)];
}

auto LocaleData::date_time_pattern(Length length) const noexcept
    -> std::string_view {
  return record_->date_time_patterns[std::to_underlying(length)];
}

auto LocaleData::month_year_pattern() const noexcept -> std::string_view {
  return record_->month_year;
}

auto LocaleData::numeric_date_pattern() const noexcept -> std::string_view {
  return record_->numeric_date;
}

auto LocaleData::abbreviated_date_pattern() const noexcept -> std::string_view {
  return record_->abbreviated_date;
}

auto LocaleData::plural_rule(PluralCategory category) const noexcept
    -> std::string_view {
  if (category == PluralCategory::other) {
    return {};
  }
  return record_->plural_rules[std::to_underlying(category)];
}

auto LocaleData::relative_pattern(Unit unit, bool future,
                                  PluralCategory category) const noexcept
    -> std::string_view {
  const Record::Counts& counts =
      record_->relative[std::to_underlying(unit)][future ? 1U : 0U];
  const std::string_view found = counts[std::to_underlying(category)];
  return found.empty() ? counts[std::to_underlying(PluralCategory::other)]
                       : found;
}

auto LocaleData::first_weekday(std::string_view region) const noexcept
    -> std::chrono::weekday {
  const std::string_view territory = region.empty() ? record_->region : region;
  for (const auto& [code, day] : first_days()) {
    if (code == territory) {
      return std::chrono::weekday{day};
    }
  }
  return std::chrono::Monday;
}

auto LocaleData::uses_twelve_hour() const noexcept -> bool {
  bool quoted = false;
  for (const char character :
       record_->time_patterns[std::to_underlying(Length::short_form)]) {
    if (character == '\'') {
      quoted = !quoted;
    } else if (!quoted && (character == 'h' || character == 'K')) {
      return true;
    }
  }
  return false;
}

auto LocaleData::format_record() const noexcept -> const FormatRecord& {
  const std::span<const Record> all = records();
  const auto index = static_cast<std::size_t>(record_ - all.data());
  const std::span<const FormatRecord> table = formats();
  for (const FormatRecord& entry : table) {
    if (entry.identifier == record_->identifier) {
      return entry;
    }
  }
  return table[std::min(index, table.size() - 1)];
}

auto LocaleData::currency_template(CurrencyForm form) const noexcept
    -> std::string_view {
  const FormatRecord& entry = format_record();
  switch (form) {
    case CurrencyForm::symbol:
      return entry.currency;
    case CurrencyForm::negative:
      return entry.currency_negative;
    case CurrencyForm::iso_code:
      return entry.currency_iso;
  }
  return entry.currency;
}

auto LocaleData::currency_symbol(std::string_view code) const noexcept
    -> std::string_view {
  const auto found = std::ranges::find(currency_codes, code);
  if (found == currency_codes.end()) {
    return code;
  }
  return format_record().currency_symbols[static_cast<std::size_t>(
      found - currency_codes.begin())];
}

auto LocaleData::currency_fraction_digits(std::string_view code) noexcept
    -> unsigned {
  for (const auto& [name, digits] : currency_digits()) {
    if (name == code) {
      return digits;
    }
  }
  return 2;
}

auto LocaleData::byte_pattern(PluralCategory category) const noexcept
    -> std::string_view {
  const FormatRecord::Counts& counts = format_record().bytes;
  const std::string_view found = counts[std::to_underlying(category)];
  return found.empty() ? counts[std::to_underlying(PluralCategory::other)]
                       : found;
}

auto LocaleData::byte_unit_pattern(std::size_t unit) const noexcept
    -> std::string_view {
  const auto& units = format_record().byte_units;
  return units[std::min(unit, units.size() - 1)];
}

auto LocaleData::zero_bytes() const noexcept -> std::string_view {
  return format_record().zero_bytes;
}

auto LocaleData::duration_pattern(Unit unit, bool abbreviated,
                                  PluralCategory category) const noexcept
    -> std::string_view {
  const FormatRecord::Counts& counts =
      format_record()
          .durations[abbreviated ? 1U : 0U][std::to_underlying(unit)];
  const std::string_view found = counts[std::to_underlying(category)];
  return found.empty() ? counts[std::to_underlying(PluralCategory::other)]
                       : found;
}

auto LocaleData::measure_pattern(std::size_t unit, UnitWidth width,
                                 PluralCategory category) const noexcept
    -> std::string_view {
  const auto& units = format_record().units;
  const FormatRecord::Counts& counts =
      units[std::min(unit, units.size() - 1)][std::to_underlying(width)];
  const std::string_view found = counts[std::to_underlying(category)];
  return found.empty() ? counts[std::to_underlying(PluralCategory::other)]
                       : found;
}

auto LocaleData::pair_pattern() const noexcept -> std::string_view {
  return format_record().pair;
}

auto LocaleData::named_day(int offset) const noexcept -> std::string_view {
  const FormatRecord& entry = format_record();
  if (offset < 0) {
    return entry.yesterday;
  }
  if (offset > 0) {
    return entry.tomorrow;
  }
  return entry.now;
}

}
