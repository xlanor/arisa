#pragma once

#include <arisa/text/locale.hpp>
#include <arisa/text/locale_data.hpp>
#include <arisa/text/number_format_style.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace arisa::text {

class Measurement {
 public:
  enum class Dimension : std::uint8_t {
    length,
    mass,
    temperature,
    volume,
    speed,
    duration,
  };

  enum class Unit : std::uint8_t {
    meters,
    kilometers,
    centimeters,
    millimeters,
    miles,
    feet,
    inches,
    yards,
    kilograms,
    grams,
    pounds,
    ounces,
    celsius,
    fahrenheit,
    liters,
    milliliters,
    kilometers_per_hour,
    miles_per_hour,
    meters_per_second,
    seconds,
    minutes,
    hours,
  };

  constexpr Measurement(double value, Unit unit) noexcept
      : value_(value), unit_(unit) {}

  constexpr auto value() const noexcept -> double { return value_; }

  constexpr auto unit() const noexcept -> Unit { return unit_; }

  static auto dimension(Unit unit) noexcept -> Dimension;

  auto converted(Unit target) const noexcept -> std::optional<Measurement>;

  friend constexpr auto operator==(const Measurement&, const Measurement&)
      -> bool = default;

 private:
  static auto coefficient(Unit unit) noexcept -> double;

  static auto constant(Unit unit) noexcept -> double;

  double value_;
  Unit unit_;
};

class MeasurementFormatStyle {
 public:
  using UnitWidth = LocaleData::UnitWidth;

  explicit MeasurementFormatStyle(
      UnitWidth width = UnitWidth::abbreviated,
      NumberFormatStyle number = NumberFormatStyle::number())
      : width_(width), number_(std::move(number)) {}

  auto locale(Locale value) const -> MeasurementFormatStyle {
    MeasurementFormatStyle changed = *this;
    changed.locale_ = std::move(value);
    return changed;
  }

  auto width() const noexcept -> UnitWidth { return width_; }

  auto format(const Measurement& measurement, const Locale& locale) const
      -> std::string;

  auto parse(std::string_view text, const Locale& locale) const
      -> std::optional<Measurement>;

  friend auto operator==(const MeasurementFormatStyle&,
                         const MeasurementFormatStyle&) -> bool = default;

 private:
  UnitWidth width_;
  NumberFormatStyle number_;
  std::optional<Locale> locale_;
};

}
