#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/provider/text_input/text_input_traits.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/number_format_style.hpp>

#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace arisa::controls {

template <class Format, class Value>
concept ParseableFormatStyle =
    requires(const Format& format, const Value& value, std::string_view text,
             const text::Locale& locale) {
      { format.format(value, locale) } -> std::convertible_to<std::string>;
      {
        format.parse(text, locale)
      } -> std::convertible_to<std::optional<Value>>;
    };

template <class Value>
class Formatter {
 public:
  using Describe = std::function<std::string(const Value&)>;
  using Read = std::function<std::optional<Value>(std::string_view)>;

  Formatter(Describe describe, Read read)
      : describe_(std::move(describe)), read_(std::move(read)) {}

  auto string(const Value& value) const -> std::string {
    return describe_(value);
  }

  auto value(std::string_view text) const -> std::optional<Value> {
    return read_(text);
  }

 private:
  Describe describe_;
  Read read_;
};

class TextValueFormat {
 public:
  template <class Value, class Format>
  static auto describe(const Format& format, const Value& value,
                       const text::Locale& locale) -> std::string {
    if constexpr (std::same_as<Format, text::NumberFormatStyle> &&
                  std::integral<Value>) {
      return format.format(static_cast<std::int64_t>(value), locale);
    } else if constexpr (std::same_as<Format, text::NumberFormatStyle>) {
      return format.format(static_cast<double>(value), locale);
    } else {
      return format.format(value, locale);
    }
  }

  template <class Value, class Format>
  static auto read(const Format& format, std::string_view text,
                   const text::Locale& locale) -> std::optional<Value> {
    if constexpr (std::same_as<Format, text::NumberFormatStyle>) {
      const std::optional<double> parsed = format.parse(text, locale);
      if (!parsed.has_value()) {
        return std::nullopt;
      }
      if constexpr (std::integral<Value>) {
        const double rounded = std::round(*parsed);
        if (rounded < static_cast<double>(std::numeric_limits<Value>::min()) ||
            rounded > static_cast<double>(std::numeric_limits<Value>::max())) {
          return std::nullopt;
        }
        return static_cast<Value>(rounded);
      } else {
        return static_cast<Value>(*parsed);
      }
    } else {
      return format.parse(text, locale);
    }
  }

  template <class Value, class Format>
  static constexpr auto parses() noexcept -> bool {
    if constexpr (std::same_as<Format, text::NumberFormatStyle>) {
      return std::is_arithmetic_v<Value> && !std::same_as<Value, bool>;
    } else {
      return ParseableFormatStyle<Format, Value>;
    }
  }

  template <class Value, class Format>
  static auto binding(composition::Binding<Value> value, Format format,
                      const text::Locale& locale)
      -> composition::Binding<std::string> {
    return composition::Binding<std::string>{
        [value, format, locale] {
          return describe<Value>(format, value.get(), locale);
        },
        [value, format, locale](std::string text) {
          if (std::optional<Value> parsed = read<Value>(format, text, locale)) {
            value.set(std::move(*parsed));
          }
        }};
  }

  template <class Value>
  static auto binding(composition::Binding<Value> value,
                      Formatter<Value> formatter)
      -> composition::Binding<std::string> {
    return composition::Binding<std::string>{
        [value, formatter] { return formatter.string(value.get()); },
        [value, formatter](std::string text) {
          if (std::optional<Value> parsed = formatter.value(text)) {
            value.set(std::move(*parsed));
          }
        }};
  }

  template <class Value>
  static constexpr auto keyboard() noexcept -> provider::KeyboardType {
    if constexpr (std::unsigned_integral<Value>) {
      return provider::KeyboardType::number_pad;
    } else if constexpr (std::is_arithmetic_v<Value>) {
      return provider::KeyboardType::numbers_and_punctuation;
    } else {
      return provider::KeyboardType::automatic;
    }
  }
};

}
