#pragma once

#include <arisa/text/locale.hpp>

#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace arisa::text {

class Interpolation {
 public:
  Interpolation() = default;
  Interpolation(const Interpolation&) = delete;
  auto operator=(const Interpolation&) -> Interpolation& = delete;
  Interpolation(Interpolation&&) = delete;
  auto operator=(Interpolation&&) -> Interpolation& = delete;
  virtual ~Interpolation() = default;

  virtual auto text(const Locale& locale) const -> std::string = 0;

  virtual auto tag() const noexcept -> const void* = 0;

  virtual auto same(const Interpolation& other) const -> bool = 0;

  virtual auto rich() const noexcept -> bool { return false; }

  friend auto operator==(const std::shared_ptr<const Interpolation>& left,
                         const std::shared_ptr<const Interpolation>& right)
      -> bool {
    if (left == nullptr || right == nullptr) {
      return left.get() == right.get();
    }
    return left.get() == right.get() || left->same(*right);
  }
};

template <class Style, class Value>
concept FormatStyleFor =
    requires(const Style& style, const Value& value, const Locale& locale) {
      { style.format(value, locale) } -> std::convertible_to<std::string>;
    };

class FormatSpecifier {
 public:
  explicit FormatSpecifier(std::string specifier)
      : specifier_(std::move(specifier)) {}

  auto specifier() const noexcept -> std::string_view { return specifier_; }

  auto format(std::int64_t value, const Locale& locale) const -> std::string;

  auto format(double value, const Locale& locale) const -> std::string;

  auto format(std::string_view value, const Locale& locale) const
      -> std::string;

  friend auto operator==(const FormatSpecifier&, const FormatSpecifier&)
      -> bool = default;

 private:
  struct Parsed {
    std::string flags;
    std::optional<int> width;
    std::optional<int> precision;
    char conversion{'@'};
  };

  auto parsed() const -> Parsed;

  static auto padded(std::string text, const Parsed& parsed) -> std::string;

  std::string specifier_;
};

class LocalizedStringKey {
 public:
  using Interpolated = std::shared_ptr<const Interpolation>;
  using Argument =
      std::variant<std::int64_t, double, std::string, Interpolated>;

  template <class Value, class Style>
    requires FormatStyleFor<Style, Value>
  class Bound final : public Interpolation {
   public:
    Bound(Value value, Style style)
        : value_(std::move(value)), style_(std::move(style)) {}

    auto text(const Locale& locale) const -> std::string override {
      return std::string{style_.format(value_, locale)};
    }

    auto tag() const noexcept -> const void* override { return &marker; }

    auto same(const Interpolation& other) const -> bool override {
      if (other.tag() != tag()) {
        return false;
      }
      const auto& theirs = static_cast<const Bound&>(other);
      if constexpr (std::equality_comparable<Value> &&
                    std::equality_comparable<Style>) {
        return value_ == theirs.value_ && style_ == theirs.style_;
      } else {
        return false;
      }
    }

   private:
    static constexpr char marker = 0;

    Value value_;
    Style style_;
  };

  explicit LocalizedStringKey(std::string key) : key_(std::move(key)) {}

  template <class... Arguments>
    requires(sizeof...(Arguments) > 0)
  LocalizedStringKey(std::string key, Arguments&&... arguments)
      : key_(std::move(key)) {
    arguments_.reserve(sizeof...(Arguments));
    (arguments_.push_back(argument(std::forward<Arguments>(arguments))), ...);
  }

  template <class Value, class Style>
    requires FormatStyleFor<Style, Value>
  static auto formatted(Value value, Style style) -> Argument {
    return Argument{Interpolated{std::make_shared<const Bound<Value, Style>>(
        std::move(value), std::move(style))}};
  }

  template <class Value, class Formatter>
    requires FormatStyleFor<Formatter, Value>
  static auto formatter(Value value, Formatter formatter) -> Argument {
    return formatted(std::move(value), std::move(formatter));
  }

  template <class Value>
    requires(std::is_arithmetic_v<Value> ||
             std::convertible_to<Value, std::string_view>)
  static auto specified(Value value, std::string specifier) -> Argument {
    if constexpr (std::integral<Value>) {
      return formatted(static_cast<std::int64_t>(value),
                       FormatSpecifier{std::move(specifier)});
    } else if constexpr (std::floating_point<Value>) {
      return formatted(static_cast<double>(value),
                       FormatSpecifier{std::move(specifier)});
    } else {
      return formatted(std::string{std::string_view{value}},
                       FormatSpecifier{std::move(specifier)});
    }
  }

  auto table(std::string name) && -> LocalizedStringKey {
    table_ = std::move(name);
    return std::move(*this);
  }

  auto key() const noexcept -> std::string_view { return key_; }

  auto table() const noexcept -> const std::optional<std::string>& {
    return table_;
  }

  auto arguments() const noexcept -> const std::vector<Argument>& {
    return arguments_;
  }

  friend auto operator==(const LocalizedStringKey&, const LocalizedStringKey&)
      -> bool = default;

 private:
  template <class Value>
  static auto argument(Value&& value) -> Argument {
    using Plain = std::remove_cvref_t<Value>;
    if constexpr (std::same_as<Plain, Argument>) {
      return std::forward<Value>(value);
    } else if constexpr (std::convertible_to<Plain, Interpolated>) {
      return Argument{Interpolated{std::forward<Value>(value)}};
    } else if constexpr (requires { value.interpolation(); }) {
      return Argument{value.interpolation()};
    } else if constexpr (std::integral<Plain> && !std::same_as<Plain, bool>) {
      return Argument{static_cast<std::int64_t>(value)};
    } else if constexpr (std::floating_point<Plain>) {
      return Argument{static_cast<double>(value)};
    } else {
      return Argument{std::string{std::forward<Value>(value)}};
    }
  }

  std::string key_;
  std::optional<std::string> table_;
  std::vector<Argument> arguments_;
};

}
