#include <arisa/text/number_format_style.hpp>
#include <arisa/text/utf8.hpp>

#include <SheenBidi/SheenBidi.h>

#include <charconv>
#include <cmath>
#include <format>
#include <string_view>
#include <system_error>

namespace arisa::text {

auto NumberFormatStyle::currency(std::string_view code) noexcept
    -> NumberFormatStyle {
  const unsigned digits = LocaleData::currency_fraction_digits(code);
  NumberFormatStyle style{Kind::currency, digits, digits};
  for (std::size_t index = 0; index < style.code_.size(); ++index) {
    const char letter = index < code.size() ? code[index] : 'X';
    style.code_[index] = letter >= 'a' && letter <= 'z'
                             ? static_cast<char>(letter - 32)
                             : letter;
  }
  return style;
}

auto NumberFormatStyle::letter(char32_t codepoint) noexcept -> bool {
  const SBGeneralCategory category = SBCodepointGetGeneralCategory(codepoint);
  return category == SBGeneralCategoryLU || category == SBGeneralCategoryLL ||
         category == SBGeneralCategoryLT || category == SBGeneralCategoryLM ||
         category == SBGeneralCategoryLO;
}

auto NumberFormatStyle::spaced(std::string_view symbol,
                               std::string_view pattern) -> std::string {
  constexpr std::string_view space = "\u00A0";
  std::string filled{pattern};
  const std::size_t at = filled.find("{1}");
  const std::size_t number = filled.find("{0}");
  if (at == std::string::npos || number == std::string::npos ||
      symbol.empty()) {
    return filled;
  }
  std::string shown{symbol};
  if (number == at + 3 &&
      letter(Utf8::decode(symbol, Utf8::previous(symbol, symbol.size()))
                 .codepoint)) {
    shown += space;
  } else if (at == number + 3 && letter(Utf8::decode(symbol, 0).codepoint)) {
    shown.insert(0, space);
  }
  filled.replace(at, 3, shown);
  return filled;
}

auto NumberFormatStyle::monetary(double value, LocaleData data) const
    -> std::string {
  const bool negative = std::signbit(value) && digits(-value) != digits(0.0);
  const std::string number =
      localized(digits(std::fabs(value)), data, grouped_);
  const LocaleData::CurrencyForm form =
      presentation_ == CurrencyPresentation::iso_code
          ? LocaleData::CurrencyForm::iso_code
          : (negative ? LocaleData::CurrencyForm::negative
                      : LocaleData::CurrencyForm::symbol);
  const std::string_view symbol =
      presentation_ == CurrencyPresentation::iso_code
          ? currency_code()
          : data.currency_symbol(currency_code());
  std::string filled = spaced(symbol, data.currency_template(form));
  if (const std::size_t at = filled.find("{0}"); at != std::string::npos) {
    filled.replace(at, 3, number);
  }
  if (const std::size_t at = filled.find("{2}"); at != std::string::npos) {
    filled.replace(at, 3, "-");
  } else if (negative) {
    filled.insert(0, "-");
  }
  return filled;
}

auto NumberFormatStyle::resolved(const Locale& locale) const -> LocaleData {
  return LocaleData::of(locale_.value_or(locale));
}

auto NumberFormatStyle::digits(double magnitude) const -> std::string {
  std::string fixed = std::format("{:.{}f}", magnitude, maximum_fraction_);
  const std::size_t point = fixed.find('.');
  if (point == std::string::npos) {
    return fixed;
  }
  std::size_t keep = fixed.size();
  while (keep > point + 1 + minimum_fraction_ && fixed[keep - 1] == '0') {
    --keep;
  }
  if (keep == point + 1) {
    keep = point;
  }
  fixed.resize(keep);
  return fixed;
}

auto NumberFormatStyle::group(std::string_view digits, LocaleData data)
    -> std::string {
  constexpr std::size_t size = 3;
  if (digits.size() < size + data.minimum_grouping_digits()) {
    return std::string{digits};
  }
  std::string grouped;
  for (std::size_t index = 0; index < digits.size(); ++index) {
    if (index != 0 && (digits.size() - index) % size == 0) {
      grouped += data.grouping_separator();
    }
    grouped += digits[index];
  }
  return grouped;
}

auto NumberFormatStyle::localized(std::string_view plain, LocaleData data,
                                  bool grouped) -> std::string {
  std::string result;
  const bool negative = plain.starts_with('-');
  if (negative) {
    plain.remove_prefix(1);
    result += data.minus_sign();
  }
  const std::size_t point = plain.find('.');
  const std::string_view whole = plain.substr(0, point);
  result += grouped ? group(whole, data) : std::string{whole};
  if (point != std::string_view::npos) {
    result += data.decimal_separator();
    result += plain.substr(point + 1);
  }
  return result;
}

auto NumberFormatStyle::affixed(std::string_view pattern,
                                std::string_view number, std::string_view sign)
    -> std::string {
  const std::size_t first = pattern.find_first_of("#0");
  const std::size_t last = pattern.find_last_of("#0");
  if (first == std::string_view::npos) {
    return std::string{number};
  }
  std::string result;
  const auto literal = [&result, sign](std::string_view part) {
    for (const char character : part) {
      if (character == '%') {
        result += sign;
      } else if (character != '\'') {
        result += character;
      }
    }
  };
  literal(pattern.substr(0, first));
  result += number;
  literal(pattern.substr(last + 1));
  return result;
}

auto NumberFormatStyle::format(double value, const Locale& locale) const
    -> std::string {
  const LocaleData data = resolved(locale);
  if (std::isnan(value)) {
    return "NaN";
  }
  if (kind_ == Kind::currency) {
    return monetary(value, data);
  }
  const double scaled = kind_ == Kind::percent ? value * 100.0 : value;
  const bool negative = std::signbit(scaled) && digits(-scaled) != digits(0.0);
  std::string number = localized(digits(std::fabs(scaled)), data, grouped_);
  if (kind_ == Kind::percent) {
    number = affixed(data.percent_pattern(), number, data.percent_sign());
  }
  if (negative) {
    return std::string{data.minus_sign()} + number;
  }
  return number;
}

auto NumberFormatStyle::format(std::int64_t value, const Locale& locale) const
    -> std::string {
  if (kind_ != Kind::number || minimum_fraction_ > 0) {
    return format(static_cast<double>(value), locale);
  }
  const LocaleData data = resolved(locale);
  const std::string plain = std::format("{}", value);
  return localized(plain, data, grouped_);
}

auto NumberFormatStyle::parse(std::string_view text, const Locale& locale) const
    -> std::optional<double> {
  const LocaleData data = resolved(locale);
  std::string plain;
  std::string_view rest = text;
  while (!rest.empty()) {
    if (rest.starts_with(data.decimal_separator())) {
      plain += '.';
      rest.remove_prefix(data.decimal_separator().size());
    } else if (rest.starts_with(data.minus_sign()) || rest.starts_with('-')) {
      plain += '-';
      rest.remove_prefix(rest.starts_with('-') ? 1 : data.minus_sign().size());
    } else if (rest.front() >= '0' && rest.front() <= '9') {
      plain += rest.front();
      rest.remove_prefix(1);
    } else if (rest.starts_with(data.grouping_separator()) && grouped_) {
      rest.remove_prefix(data.grouping_separator().size());
    } else if (rest.starts_with(data.percent_sign()) || rest.front() == ' ' ||
               rest.front() == '+') {
      rest.remove_prefix(rest.starts_with(data.percent_sign())
                             ? data.percent_sign().size()
                             : 1);
    } else if (kind_ == Kind::currency) {
      rest.remove_prefix(1);
    } else {
      return std::nullopt;
    }
  }
  if (plain.empty()) {
    return std::nullopt;
  }
  double value = 0.0;
  const auto [end, error] =
      std::from_chars(plain.data(), plain.data() + plain.size(), value);
  if (error != std::errc{} || end != plain.data() + plain.size()) {
    return std::nullopt;
  }
  return kind_ == Kind::percent ? value / 100.0 : value;
}

auto NumberFormatStyle::filled(std::string_view pattern, std::string_view text)
    -> std::optional<std::string> {
  const auto plain = [](std::string_view source) {
    std::string result;
    result.reserve(source.size());
    for (std::size_t at = 0; at < source.size();) {
      if (source.substr(at).starts_with(" ")) {
        result.push_back(' ');
        at += std::string_view{" "}.size();
      } else if (source.substr(at).starts_with(" ")) {
        result.push_back(' ');
        at += std::string_view{" "}.size();
      } else {
        result.push_back(source[at]);
        ++at;
      }
    }
    return result;
  };
  const std::size_t at = pattern.find("{0}");
  if (at == std::string_view::npos) {
    return std::nullopt;
  }
  const std::string prefix = plain(pattern.substr(0, at));
  const std::string suffix = plain(pattern.substr(at + 3));
  const std::string shown = plain(text);
  if (shown.size() < prefix.size() + suffix.size() ||
      !shown.starts_with(prefix) || !shown.ends_with(suffix)) {
    return std::nullopt;
  }
  return shown.substr(prefix.size(),
                      shown.size() - prefix.size() - suffix.size());
}

}
