#include <arisa/text/byte_count_format_style.hpp>
#include <arisa/text/number_format_style.hpp>
#include <arisa/text/plural_rules.hpp>

#include <cmath>
#include <cstdlib>

namespace arisa::text {

auto ByteCountFormatStyle::base() const noexcept -> double {
  return style_ == Style::memory || style_ == Style::binary ? 1024.0 : 1000.0;
}

auto ByteCountFormatStyle::chosen(std::int64_t bytes) const noexcept
    -> std::size_t {
  const double magnitude = std::fabs(static_cast<double>(bytes));
  const bool decimal = base() == 1000.0;
  std::size_t best = 0;
  bool found = false;
  double scale = 1.0;
  for (std::size_t unit = 0; unit < unit_count; ++unit) {
    const auto flag = static_cast<Unit>(1U << unit);
    if (allowed_.contains(flag)) {
      const double value = magnitude / scale;
      const bool reaches =
          unit == 0 || (decimal ? value >= threshold : value >= 1.0);
      if (reaches || !found) {
        best = unit;
        found = true;
      }
    }
    scale *= base();
  }
  return best;
}

auto ByteCountFormatStyle::counted(std::int64_t bytes, LocaleData data,
                                   const Locale& locale) -> std::string {
  std::string pattern{data.byte_pattern(PluralRules{locale}.category(bytes))};
  const std::string number = NumberFormatStyle::number().format(bytes, locale);
  if (const std::size_t at = pattern.find("{0}"); at != std::string::npos) {
    pattern.replace(at, 3, number);
  }
  return pattern;
}

auto ByteCountFormatStyle::format(std::int64_t bytes,
                                  const Locale& locale) const -> std::string {
  const Locale& used = locale_.value_or(locale);
  const LocaleData data = LocaleData::of(used);
  if (bytes == 0 && spells_out_zero_) {
    return std::string{data.zero_bytes()};
  }
  const std::size_t unit = chosen(bytes);
  std::string shown;
  if (unit == 0) {
    shown = counted(bytes, data, used);
  } else {
    const double value = static_cast<double>(bytes) /
                         std::pow(base(), static_cast<double>(unit));
    const unsigned fraction = unit == 1 ? 0U : (unit == 2 ? 1U : 2U);
    const std::string number = NumberFormatStyle::number()
                                   .fraction_length(0, fraction)
                                   .format(value, used);
    shown = data.byte_unit_pattern(unit - 1);
    if (const std::size_t at = shown.find("{0}"); at != std::string::npos) {
      shown.replace(at, 3, number);
    }
  }
  if (includes_actual_ && unit != 0) {
    shown += " (" + counted(bytes, data, used) + ")";
  }
  return shown;
}

auto ByteCountFormatStyle::parse(std::string_view text,
                                 const Locale& locale) const
    -> std::optional<std::int64_t> {
  const Locale& used = locale_.value_or(locale);
  const LocaleData data = LocaleData::of(used);
  if (text == data.zero_bytes()) {
    return 0;
  }
  const NumberFormatStyle number = NumberFormatStyle::number();
  for (std::size_t unit = 0; unit < 5; ++unit) {
    if (const std::optional<std::string> middle =
            NumberFormatStyle::filled(data.byte_unit_pattern(unit), text)) {
      if (const std::optional<double> value = number.parse(*middle, used)) {
        return std::llround(*value *
                            std::pow(base(), static_cast<double>(unit + 1)));
      }
    }
  }
  for (const PluralCategory category :
       {PluralCategory::zero, PluralCategory::one, PluralCategory::two,
        PluralCategory::few, PluralCategory::many, PluralCategory::other}) {
    if (const std::optional<std::string> middle =
            NumberFormatStyle::filled(data.byte_pattern(category), text)) {
      if (const std::optional<double> value = number.parse(*middle, used)) {
        return std::llround(*value);
      }
    }
  }
  if (const std::optional<double> value = number.parse(text, used)) {
    return std::llround(*value);
  }
  return std::nullopt;
}

}
