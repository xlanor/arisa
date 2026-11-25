#pragma once

#include <arisa/text/locale.hpp>
#include <arisa/text/locale_data.hpp>
#include <arisa/text/plural_category.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace arisa::text {

class PluralRules {
 public:
  explicit PluralRules(const Locale& locale) noexcept
      : data_(LocaleData::of(locale)) {}

  explicit PluralRules(LocaleData data) noexcept : data_(data) {}

  auto category(std::int64_t value) const noexcept -> PluralCategory;

  auto category(double value, unsigned fraction_digits) const noexcept
      -> PluralCategory;

  static auto name(PluralCategory category) noexcept -> std::string_view;

  static auto parse(std::string_view name) noexcept
      -> std::optional<PluralCategory>;

 private:
  struct Operands {
    double n{0.0};
    std::uint64_t i{0};
    unsigned v{0};
    unsigned w{0};
    std::uint64_t f{0};
    std::uint64_t t{0};
  };

  class Rule {
   public:
    Rule(std::string_view source, const Operands& operands) noexcept
        : source_(source), operands_(operands) {}

    auto holds() noexcept -> bool;

   private:
    auto condition() noexcept -> bool;
    auto conjunction() noexcept -> bool;
    auto relation() noexcept -> bool;
    auto operand() noexcept -> double;
    auto number() noexcept -> double;
    auto accept(std::string_view token) noexcept -> bool;
    void skip() noexcept;

    std::string_view source_;
    std::size_t at_{0};
    const Operands& operands_;
  };

  auto select(const Operands& operands) const noexcept -> PluralCategory;

  LocaleData data_;
};

}
