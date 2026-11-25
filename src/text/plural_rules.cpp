#include <arisa/text/plural_rules.hpp>

#include <array>
#include <cmath>
#include <utility>

namespace arisa::text {

auto PluralRules::category(std::int64_t value) const noexcept
    -> PluralCategory {
  const std::uint64_t magnitude =
      value < 0 ? static_cast<std::uint64_t>(-(value + 1)) + 1U
                : static_cast<std::uint64_t>(value);
  return select(Operands{.n = static_cast<double>(magnitude),
                         .i = magnitude,
                         .v = 0,
                         .w = 0,
                         .f = 0,
                         .t = 0});
}

auto PluralRules::category(double value,
                           unsigned fraction_digits) const noexcept
    -> PluralCategory {
  const unsigned digits = std::min(fraction_digits, 9U);
  double scale = 1.0;
  for (unsigned index = 0; index < digits; ++index) {
    scale *= 10.0;
  }
  const double magnitude = std::fabs(value);
  const double rounded = std::round(magnitude * scale) / scale;
  const auto whole = static_cast<std::uint64_t>(std::floor(rounded));
  auto fraction = static_cast<std::uint64_t>(
      std::llround((rounded - static_cast<double>(whole)) * scale));
  Operands operands{.n = rounded,
                    .i = whole,
                    .v = digits,
                    .w = digits,
                    .f = fraction,
                    .t = fraction};
  while (operands.w > 0 && operands.t % 10U == 0U) {
    operands.t /= 10U;
    --operands.w;
  }
  return select(operands);
}

auto PluralRules::select(const Operands& operands) const noexcept
    -> PluralCategory {
  for (const PluralCategory each :
       {PluralCategory::zero, PluralCategory::one, PluralCategory::two,
        PluralCategory::few, PluralCategory::many}) {
    const std::string_view rule = data_.plural_rule(each);
    if (!rule.empty() && Rule{rule, operands}.holds()) {
      return each;
    }
  }
  return PluralCategory::other;
}

auto PluralRules::name(PluralCategory category) noexcept -> std::string_view {
  constexpr std::array<std::string_view, 6> names{"zero", "one",  "two",
                                                  "few",  "many", "other"};
  return names[std::to_underlying(category)];
}

auto PluralRules::parse(std::string_view name) noexcept
    -> std::optional<PluralCategory> {
  for (const PluralCategory each :
       {PluralCategory::zero, PluralCategory::one, PluralCategory::two,
        PluralCategory::few, PluralCategory::many, PluralCategory::other}) {
    if (PluralRules::name(each) == name) {
      return each;
    }
  }
  return std::nullopt;
}

auto PluralRules::Rule::holds() noexcept -> bool { return condition(); }

void PluralRules::Rule::skip() noexcept {
  while (at_ < source_.size() && source_[at_] == ' ') {
    ++at_;
  }
}

auto PluralRules::Rule::accept(std::string_view token) noexcept -> bool {
  skip();
  if (source_.substr(at_).starts_with(token)) {
    at_ += token.size();
    return true;
  }
  return false;
}

auto PluralRules::Rule::condition() noexcept -> bool {
  bool result = conjunction();
  while (accept("or")) {
    const bool next = conjunction();
    result = result || next;
  }
  return result;
}

auto PluralRules::Rule::conjunction() noexcept -> bool {
  bool result = relation();
  while (accept("and")) {
    const bool next = relation();
    result = result && next;
  }
  return result;
}

auto PluralRules::Rule::number() noexcept -> double {
  skip();
  double value = 0.0;
  while (at_ < source_.size() && source_[at_] >= '0' && source_[at_] <= '9') {
    value = (value * 10.0) + static_cast<double>(source_[at_] - '0');
    ++at_;
  }
  return value;
}

auto PluralRules::Rule::operand() noexcept -> double {
  skip();
  double value = 0.0;
  if (at_ < source_.size()) {
    switch (source_[at_]) {
      case 'n':
        value = operands_.n;
        break;
      case 'i':
        value = static_cast<double>(operands_.i);
        break;
      case 'v':
        value = static_cast<double>(operands_.v);
        break;
      case 'w':
        value = static_cast<double>(operands_.w);
        break;
      case 'f':
        value = static_cast<double>(operands_.f);
        break;
      case 't':
        value = static_cast<double>(operands_.t);
        break;
      default:
        break;
    }
    ++at_;
  }
  if (accept("%")) {
    const double divisor = number();
    if (divisor > 0.0) {
      value = std::fmod(value, divisor);
    }
  }
  return value;
}

auto PluralRules::Rule::relation() noexcept -> bool {
  const double value = operand();
  bool negated = false;
  if (accept("!=")) {
    negated = true;
  } else {
    static_cast<void>(accept("="));
  }
  bool matched = false;
  do {
    const double low = number();
    double high = low;
    if (accept("..")) {
      high = number();
    }
    if (value >= low && value <= high && value == std::floor(value)) {
      matched = true;
    }
  } while (accept(","));
  return negated ? !matched : matched;
}

}
