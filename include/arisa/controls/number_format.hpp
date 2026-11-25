#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/provider/text_input/text_input_traits.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/locale_data.hpp>
#include <arisa/text/number_format_style.hpp>

#include <charconv>
#include <concepts>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace arisa::controls {

template <class T>
concept FormattedNumber = std::is_arithmetic_v<T> && !std::same_as<T, bool>;

class NumberFormat {
 public:
  static constexpr auto number() noexcept -> NumberFormat {
    return NumberFormat{};
  }

  constexpr auto precision(int fraction_length) const noexcept -> NumberFormat {
    NumberFormat changed = *this;
    changed.fraction_length_ = fraction_length < 0 ? 0 : fraction_length;
    return changed;
  }

  constexpr auto fraction_length() const noexcept -> std::optional<int> {
    return fraction_length_;
  }

  template <FormattedNumber T>
  auto format(T value) const -> std::string {
    if constexpr (std::floating_point<T>) {
      if (fraction_length_.has_value()) {
        return std::format("{:.{}f}", value, *fraction_length_);
      }
    }
    return std::format("{}", value);
  }

  template <FormattedNumber T>
  auto parse(std::string_view text) const -> std::optional<T> {
    while (!text.empty() && text.front() == ' ') {
      text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ') {
      text.remove_suffix(1);
    }
    if (text.starts_with('+')) {
      text.remove_prefix(1);
    }
    if (text.empty()) {
      return std::nullopt;
    }
    T value{};
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
      return std::nullopt;
    }
    return value;
  }

  template <FormattedNumber T>
  auto format(T value, const text::Locale& locale) const -> std::string {
    return text::NumberFormatStyle::localized(
        format(value), text::LocaleData::of(locale), false);
  }

  template <FormattedNumber T>
  auto parse(std::string_view text, const text::Locale& locale) const
      -> std::optional<T> {
    const text::LocaleData data = text::LocaleData::of(locale);
    std::string plain;
    plain.reserve(text.size());
    while (!text.empty()) {
      if (text.starts_with(data.decimal_separator())) {
        plain += '.';
        text.remove_prefix(data.decimal_separator().size());
      } else if (text.starts_with(data.minus_sign()) &&
                 !text.starts_with('-')) {
        plain += '-';
        text.remove_prefix(data.minus_sign().size());
      } else {
        plain += text.front();
        text.remove_prefix(1);
      }
    }
    return parse<T>(std::string_view{plain});
  }

  template <FormattedNumber T>
  auto binding(composition::Binding<T> value, const text::Locale& locale) const
      -> composition::Binding<std::string> {
    return composition::Binding<std::string>{
        [format = *this, value, locale] {
          return format.format(value.get(), locale);
        },
        [format = *this, value, locale](std::string text) {
          if (const std::optional<T> parsed = format.parse<T>(text, locale)) {
            value.set(*parsed);
          }
        }};
  }

  template <FormattedNumber T>
  auto binding(composition::Binding<T> value) const
      -> composition::Binding<std::string> {
    return composition::Binding<std::string>{
        [format = *this, value] { return format.format(value.get()); },
        [format = *this, value](std::string text) {
          if (const std::optional<T> parsed = format.parse<T>(text)) {
            value.set(*parsed);
          }
        }};
  }

  template <FormattedNumber T>
  static constexpr auto keyboard() noexcept -> provider::KeyboardType {
    if constexpr (std::unsigned_integral<T>) {
      return provider::KeyboardType::number_pad;
    } else {
      return provider::KeyboardType::numbers_and_punctuation;
    }
  }

  friend constexpr auto operator==(const NumberFormat&, const NumberFormat&)
      -> bool = default;

 private:
  constexpr NumberFormat() noexcept = default;

  std::optional<int> fraction_length_;
};

}
