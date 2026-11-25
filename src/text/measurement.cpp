#include <arisa/text/measurement.hpp>
#include <arisa/text/plural_rules.hpp>

#include <utility>

namespace arisa::text {

auto Measurement::dimension(Unit unit) noexcept -> Dimension {
  switch (unit) {
    case Unit::meters:
    case Unit::kilometers:
    case Unit::centimeters:
    case Unit::millimeters:
    case Unit::miles:
    case Unit::feet:
    case Unit::inches:
    case Unit::yards:
      return Dimension::length;
    case Unit::kilograms:
    case Unit::grams:
    case Unit::pounds:
    case Unit::ounces:
      return Dimension::mass;
    case Unit::celsius:
    case Unit::fahrenheit:
      return Dimension::temperature;
    case Unit::liters:
    case Unit::milliliters:
      return Dimension::volume;
    case Unit::kilometers_per_hour:
    case Unit::miles_per_hour:
    case Unit::meters_per_second:
      return Dimension::speed;
    case Unit::seconds:
    case Unit::minutes:
    case Unit::hours:
      return Dimension::duration;
  }
  return Dimension::length;
}

auto Measurement::coefficient(Unit unit) noexcept -> double {
  switch (unit) {
    case Unit::meters:
      return 1.0;
    case Unit::kilometers:
      return 1000.0;
    case Unit::centimeters:
      return 0.01;
    case Unit::millimeters:
      return 0.001;
    case Unit::miles:
      return 1609.344;
    case Unit::feet:
      return 0.3048;
    case Unit::inches:
      return 0.0254;
    case Unit::yards:
      return 0.9144;
    case Unit::kilograms:
      return 1.0;
    case Unit::grams:
      return 0.001;
    case Unit::pounds:
      return 0.45359237;
    case Unit::ounces:
      return 0.028349523125;
    case Unit::celsius:
      return 1.0;
    case Unit::fahrenheit:
      return 5.0 / 9.0;
    case Unit::liters:
      return 1.0;
    case Unit::milliliters:
      return 0.001;
    case Unit::kilometers_per_hour:
      return 1000.0 / 3600.0;
    case Unit::miles_per_hour:
      return 1609.344 / 3600.0;
    case Unit::meters_per_second:
      return 1.0;
    case Unit::seconds:
      return 1.0;
    case Unit::minutes:
      return 60.0;
    case Unit::hours:
      return 3600.0;
  }
  return 1.0;
}

auto Measurement::constant(Unit unit) noexcept -> double {
  switch (unit) {
    case Unit::celsius:
      return 273.15;
    case Unit::fahrenheit:
      return 255.37222222222222;
    default:
      return 0.0;
  }
}

auto Measurement::converted(Unit target) const noexcept
    -> std::optional<Measurement> {
  if (dimension(target) != dimension(unit_)) {
    return std::nullopt;
  }
  const double base = (value_ * coefficient(unit_)) + constant(unit_);
  return Measurement{(base - constant(target)) / coefficient(target), target};
}

auto MeasurementFormatStyle::format(const Measurement& measurement,
                                    const Locale& locale) const -> std::string {
  const Locale& used = locale_.value_or(locale);
  const LocaleData data = LocaleData::of(used);
  const std::string number = number_.format(measurement.value(), used);
  const std::string_view decimal = data.decimal_separator();
  const std::size_t point = number.find(decimal);
  const auto digits =
      point == std::string::npos
          ? 0U
          : static_cast<unsigned>(number.size() - point - decimal.size());
  const PluralCategory category =
      digits == 0 && measurement.value() ==
                         static_cast<double>(
                             static_cast<std::int64_t>(measurement.value()))
          ? PluralRules{used}.category(
                static_cast<std::int64_t>(measurement.value()))
          : PluralRules{used}.category(measurement.value(), digits);
  std::string pattern{data.measure_pattern(
      static_cast<std::size_t>(std::to_underlying(measurement.unit())), width_,
      category)};
  if (const std::size_t at = pattern.find("{0}"); at != std::string::npos) {
    pattern.replace(at, 3, number);
  }
  return pattern;
}

auto MeasurementFormatStyle::parse(std::string_view text,
                                   const Locale& locale) const
    -> std::optional<Measurement> {
  const Locale& used = locale_.value_or(locale);
  const LocaleData data = LocaleData::of(used);
  for (std::uint8_t unit = 0;
       unit <= std::to_underlying(Measurement::Unit::hours); ++unit) {
    for (const PluralCategory category :
         {PluralCategory::one, PluralCategory::other, PluralCategory::zero,
          PluralCategory::two, PluralCategory::few, PluralCategory::many}) {
      const std::optional<std::string> middle = NumberFormatStyle::filled(
          data.measure_pattern(unit, width_, category), text);
      if (!middle.has_value()) {
        continue;
      }
      if (const std::optional<double> value = number_.parse(*middle, used)) {
        return Measurement{*value, static_cast<Measurement::Unit>(unit)};
      }
    }
  }
  return std::nullopt;
}

}
