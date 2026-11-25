#include <arisa/components/text.hpp>

#include <algorithm>
#include <cmath>

namespace arisa::components {

auto Text::Clock::format(Date now, const text::Locale&) const -> std::string {
  const double seconds =
      std::chrono::duration_cast<SystemFormatStyle::Duration>(
          now > anchor_ ? now - anchor_ : anchor_ - now)
          .count();
  const double shown =
      now >= anchor_ ? std::floor(seconds + 1e-9) : std::ceil(seconds - 1e-9);
  return SystemFormatStyle::clock(SystemFormatStyle::Duration{shown}, true,
                                  false, false,
                                  SystemFormatStyle::Duration{1.0});
}

auto Text::Clock::discrete_input(Date after) const -> std::optional<Date> {
  const auto unit = std::chrono::seconds{1};
  if (after >= anchor_) {
    const auto steps = (after - anchor_) / unit;
    return anchor_ + ((steps + 1) * unit);
  }
  const auto steps = (anchor_ - after) / unit;
  const Date boundary = anchor_ - (steps * unit);
  return boundary > after ? boundary : anchor_ - ((steps - 1) * unit);
}

auto Text::Paused::format(Date now, const text::Locale& locale) const
    -> std::string {
  const Date shown = pause_.has_value() ? std::min(now, *pause_) : now;
  return timer_.format(shown, locale);
}

auto Text::Paused::discrete_input(Date after) const -> std::optional<Date> {
  if (pause_.has_value() && after >= *pause_) {
    return std::nullopt;
  }
  const std::optional<Date> next = timer_.discrete_input(after);
  if (next.has_value() && pause_.has_value() && *next > *pause_) {
    return std::nullopt;
  }
  return next;
}

auto Text::Interpolation::text(const text::Locale& locale) const
    -> std::string {
  return std::string{
      content_->resolved(composition::TextAttributes{}, locale).text()};
}

auto Text::Interpolation::same(const text::Interpolation& other) const -> bool {
  if (other.tag() != tag()) {
    return false;
  }
  return content_->equals(static_cast<const Interpolation&>(other).content());
}

}
