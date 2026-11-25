#pragma once

#include <arisa/text/locale.hpp>
#include <arisa/text/locale_data.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::text {

class NumberFormatStyle {
 public:
  enum class Kind : std::uint8_t {
    number,
    percent,
    currency,
  };

  enum class CurrencyPresentation : std::uint8_t {
    standard,
    iso_code,
  };

  static constexpr auto number() noexcept -> NumberFormatStyle {
    return NumberFormatStyle{Kind::number, 0, 6};
  }

  static constexpr auto percent() noexcept -> NumberFormatStyle {
    return NumberFormatStyle{Kind::percent, 0, 6};
  }

  static auto currency(std::string_view code) noexcept -> NumberFormatStyle;

  auto presentation(CurrencyPresentation value) const noexcept
      -> NumberFormatStyle {
    NumberFormatStyle changed = *this;
    changed.presentation_ = value;
    return changed;
  }

  auto currency_code() const -> std::string_view {
    return std::string_view{code_.data(), code_.size()};
  }

  constexpr auto currency_presentation() const noexcept
      -> CurrencyPresentation {
    return presentation_;
  }

  constexpr auto fraction_length(unsigned exact) const noexcept
      -> NumberFormatStyle {
    return fraction_length(exact, exact);
  }

  constexpr auto fraction_length(unsigned minimum,
                                 unsigned maximum) const noexcept
      -> NumberFormatStyle {
    NumberFormatStyle changed = *this;
    changed.minimum_fraction_ =
        minimum > maximum_digits ? maximum_digits : minimum;
    changed.maximum_fraction_ =
        maximum < changed.minimum_fraction_
            ? changed.minimum_fraction_
            : (maximum > maximum_digits ? maximum_digits : maximum);
    return changed;
  }

  constexpr auto grouping(bool is_grouped) const noexcept -> NumberFormatStyle {
    NumberFormatStyle changed = *this;
    changed.grouped_ = is_grouped;
    return changed;
  }

  auto locale(Locale value) const -> NumberFormatStyle {
    NumberFormatStyle changed = *this;
    changed.locale_ = std::move(value);
    return changed;
  }

  constexpr auto kind() const noexcept -> Kind { return kind_; }

  constexpr auto minimum_fraction_length() const noexcept -> unsigned {
    return minimum_fraction_;
  }

  constexpr auto maximum_fraction_length() const noexcept -> unsigned {
    return maximum_fraction_;
  }

  constexpr auto grouped() const noexcept -> bool { return grouped_; }

  auto format(double value, const Locale& locale) const -> std::string;

  auto format(std::int64_t value, const Locale& locale) const -> std::string;

  auto parse(std::string_view text, const Locale& locale) const
      -> std::optional<double>;

  static auto group(std::string_view digits, LocaleData data) -> std::string;

  static auto filled(std::string_view pattern, std::string_view text)
      -> std::optional<std::string>;

  static auto localized(std::string_view plain, LocaleData data, bool grouped)
      -> std::string;

  friend auto operator==(const NumberFormatStyle&, const NumberFormatStyle&)
      -> bool = default;

 private:
  static constexpr unsigned maximum_digits = 15;

  constexpr NumberFormatStyle(Kind kind, unsigned minimum,
                              unsigned maximum) noexcept
      : kind_(kind), minimum_fraction_(minimum), maximum_fraction_(maximum) {}

  auto resolved(const Locale& locale) const -> LocaleData;

  auto digits(double magnitude) const -> std::string;

  static auto affixed(std::string_view pattern, std::string_view number,
                      std::string_view sign) -> std::string;

  auto monetary(double value, LocaleData data) const -> std::string;

  static auto spaced(std::string_view symbol, std::string_view pattern)
      -> std::string;

  static auto letter(char32_t codepoint) noexcept -> bool;

  Kind kind_;
  unsigned minimum_fraction_;
  unsigned maximum_fraction_;
  bool grouped_{true};
  std::optional<Locale> locale_;
  std::array<char, 3> code_{};
  CurrencyPresentation presentation_{CurrencyPresentation::standard};
};

}
