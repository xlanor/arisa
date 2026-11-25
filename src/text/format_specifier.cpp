#include <arisa/text/localized_string_key.hpp>

#include <cmath>
#include <format>

namespace arisa::text {

auto FormatSpecifier::parsed() const -> Parsed {
  Parsed result;
  std::string_view rest = specifier_;
  if (rest.starts_with('%')) {
    rest.remove_prefix(1);
  }
  while (!rest.empty() &&
         (rest.front() == '-' || rest.front() == '+' || rest.front() == ' ' ||
          rest.front() == '0' || rest.front() == '#')) {
    result.flags += rest.front();
    rest.remove_prefix(1);
  }
  const auto number = [&rest]() -> std::optional<int> {
    if (rest.empty() || rest.front() < '0' || rest.front() > '9') {
      return std::nullopt;
    }
    int value = 0;
    while (!rest.empty() && rest.front() >= '0' && rest.front() <= '9') {
      value = (value * 10) + (rest.front() - '0');
      rest.remove_prefix(1);
    }
    return value;
  };
  result.width = number();
  if (rest.starts_with('.')) {
    rest.remove_prefix(1);
    result.precision = number().value_or(0);
  }
  while (!rest.empty() && (rest.front() == 'l' || rest.front() == 'h' ||
                           rest.front() == 'q' || rest.front() == 'z')) {
    rest.remove_prefix(1);
  }
  if (!rest.empty()) {
    result.conversion = rest.front();
  }
  return result;
}

auto FormatSpecifier::padded(std::string text, const Parsed& parsed)
    -> std::string {
  if (!parsed.width.has_value() ||
      text.size() >= static_cast<std::size_t>(*parsed.width)) {
    return text;
  }
  const std::size_t missing =
      static_cast<std::size_t>(*parsed.width) - text.size();
  if (parsed.flags.contains('-')) {
    return text + std::string(missing, ' ');
  }
  if (parsed.flags.contains('0') && parsed.conversion != 's' &&
      parsed.conversion != '@') {
    const std::size_t sign =
        !text.empty() && (text.front() == '-' || text.front() == '+') ? 1 : 0;
    text.insert(sign, std::string(missing, '0'));
    return text;
  }
  return std::string(missing, ' ') + text;
}

auto FormatSpecifier::format(std::int64_t value, const Locale& locale) const
    -> std::string {
  const Parsed spec = parsed();
  switch (spec.conversion) {
    case 'f':
    case 'F':
    case 'e':
    case 'E':
    case 'g':
    case 'G':
      return format(static_cast<double>(value), locale);
    case 'x':
      return padded(std::format("{:x}", value), spec);
    case 'X':
      return padded(std::format("{:X}", value), spec);
    case 'o':
      return padded(std::format("{:o}", value), spec);
    default:
      break;
  }
  std::string text = std::format("{}", value);
  if (spec.flags.contains('+') && value >= 0) {
    text.insert(0, "+");
  }
  return padded(std::move(text), spec);
}

auto FormatSpecifier::format(double value, const Locale& locale) const
    -> std::string {
  const Parsed spec = parsed();
  const int precision = spec.precision.value_or(6);
  std::string text;
  switch (spec.conversion) {
    case 'd':
    case 'i':
    case 'u':
      return format(static_cast<std::int64_t>(std::trunc(value)), locale);
    case 'e':
      text = std::format("{:.{}e}", value, precision);
      break;
    case 'E':
      text = std::format("{:.{}E}", value, precision);
      break;
    case 'g':
    case 'G':
      text = std::format("{:.{}g}", value, precision == 0 ? 1 : precision);
      break;
    case 'f':
    case 'F':
      text = std::format("{:.{}f}", value, precision);
      break;
    default:
      text = std::format("{}", value);
      break;
  }
  if (spec.flags.contains('+') && !std::signbit(value)) {
    text.insert(0, "+");
  }
  return padded(std::move(text), spec);
}

auto FormatSpecifier::format(std::string_view value, const Locale&) const
    -> std::string {
  const Parsed spec = parsed();
  std::string text{value};
  if (spec.precision.has_value() &&
      text.size() > static_cast<std::size_t>(*spec.precision)) {
    text.resize(static_cast<std::size_t>(*spec.precision));
  }
  return padded(std::move(text), spec);
}

}
