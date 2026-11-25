#pragma once

#include <arisa/text/locale.hpp>
#include <arisa/text/plural_category.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace arisa::text {

class LocaleData {
 public:
  struct Record;
  struct FormatRecord;

  enum class Width : std::uint8_t {
    wide,
    abbreviated,
    short_form,
    narrow,
  };

  enum class Context : std::uint8_t {
    format,
    standalone,
  };

  enum class Length : std::uint8_t {
    full,
    long_form,
    medium,
    short_form,
  };

  enum class Unit : std::uint8_t {
    second,
    minute,
    hour,
    day,
    week,
    month,
    year,
  };

  enum class CurrencyForm : std::uint8_t {
    symbol,
    negative,
    iso_code,
  };

  enum class UnitWidth : std::uint8_t {
    wide,
    abbreviated,
    narrow,
  };

  static constexpr std::size_t byte_unit_count = 5;
  static constexpr std::size_t measure_unit_count = 22;

  static auto of(const Locale& locale) noexcept -> LocaleData;

  static auto bundled() noexcept -> std::span<const Record>;

  static auto at(std::size_t index) noexcept -> LocaleData;

  auto identifier() const noexcept -> std::string_view;

  auto region() const noexcept -> std::string_view;

  auto decimal_separator() const noexcept -> std::string_view;

  auto grouping_separator() const noexcept -> std::string_view;

  auto minus_sign() const noexcept -> std::string_view;

  auto plus_sign() const noexcept -> std::string_view;

  auto percent_sign() const noexcept -> std::string_view;

  auto decimal_pattern() const noexcept -> std::string_view;

  auto percent_pattern() const noexcept -> std::string_view;

  auto minimum_grouping_digits() const noexcept -> unsigned;

  auto month(std::chrono::month month, Width width,
             Context context = Context::format) const noexcept
      -> std::string_view;

  auto weekday(std::chrono::weekday weekday, Width width) const noexcept
      -> std::string_view;

  auto day_period(bool after_noon) const noexcept -> std::string_view;

  auto date_pattern(Length length) const noexcept -> std::string_view;

  auto time_pattern(Length length) const noexcept -> std::string_view;

  auto date_time_pattern(Length length) const noexcept -> std::string_view;

  auto month_year_pattern() const noexcept -> std::string_view;

  auto numeric_date_pattern() const noexcept -> std::string_view;

  auto abbreviated_date_pattern() const noexcept -> std::string_view;

  auto plural_rule(PluralCategory category) const noexcept -> std::string_view;

  auto relative_pattern(Unit unit, bool future,
                        PluralCategory category) const noexcept
      -> std::string_view;

  auto first_weekday(std::string_view region = {}) const noexcept
      -> std::chrono::weekday;

  auto currency_template(CurrencyForm form) const noexcept -> std::string_view;

  auto currency_symbol(std::string_view code) const noexcept
      -> std::string_view;

  static auto currency_fraction_digits(std::string_view code) noexcept
      -> unsigned;

  auto byte_pattern(PluralCategory category) const noexcept -> std::string_view;

  auto byte_unit_pattern(std::size_t unit) const noexcept -> std::string_view;

  auto zero_bytes() const noexcept -> std::string_view;

  auto duration_pattern(Unit unit, bool abbreviated,
                        PluralCategory category) const noexcept
      -> std::string_view;

  auto measure_pattern(std::size_t unit, UnitWidth width,
                       PluralCategory category) const noexcept
      -> std::string_view;

  auto pair_pattern() const noexcept -> std::string_view;

  auto named_day(int offset) const noexcept -> std::string_view;

  auto uses_twelve_hour() const noexcept -> bool;

  friend auto operator==(const LocaleData& left, const LocaleData& right)
      -> bool {
    return left.record_ == right.record_;
  }

 private:
  explicit LocaleData(const Record& record) noexcept : record_(&record) {}

  static auto records() noexcept -> std::span<const Record>;

  static auto first_days() noexcept
      -> std::span<const std::pair<std::string_view, unsigned>>;

  static auto formats() noexcept -> std::span<const FormatRecord>;

  static auto currency_digits() noexcept
      -> std::span<const std::pair<std::string_view, unsigned>>;

  static constexpr std::array<std::string_view, 18> currency_codes{
      "USD", "EUR", "JPY", "GBP", "CNY", "KRW", "CAD", "AUD", "CHF",
      "HKD", "TWD", "BRL", "RUB", "INR", "MXN", "ILS", "EGP", "SEK"};

  auto format_record() const noexcept -> const FormatRecord&;

  static auto find(std::string_view identifier) noexcept
      -> std::optional<LocaleData>;

  static auto identifier_for(const Locale& locale) noexcept -> std::string_view;

  const Record* record_;
};

}
