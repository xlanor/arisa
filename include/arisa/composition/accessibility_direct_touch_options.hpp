#pragma once

#include <arisa/support/option_set.hpp>

#include <cstdint>

namespace arisa::composition {

class AccessibilityDirectTouchOptions
    : public support::OptionSet<AccessibilityDirectTouchOptions> {
 public:
  static const AccessibilityDirectTouchOptions silent_on_touch;
  static const AccessibilityDirectTouchOptions requires_activation;

  constexpr AccessibilityDirectTouchOptions() noexcept = default;

  constexpr explicit AccessibilityDirectTouchOptions(std::uint8_t bits) noexcept
      : OptionSet(bits) {}
};

inline constexpr AccessibilityDirectTouchOptions
    AccessibilityDirectTouchOptions::silent_on_touch{1U};
inline constexpr AccessibilityDirectTouchOptions
    AccessibilityDirectTouchOptions::requires_activation{2U};

}
