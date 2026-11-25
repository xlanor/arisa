#pragma once

#include <arisa/provider/haptics/haptic_strength.hpp>
#include <arisa/provider/haptics/rumble_pulse.hpp>
#include <arisa/provider/haptics/rumble_shape.hpp>
#include <arisa/provider/haptics/sensory_feedback.hpp>

#include <chrono>

namespace arisa::provider::horizon::ounce {

class OunceRumble {
 public:
  static constexpr float low_band_minimum = 40.875885F;
  static constexpr float low_band_maximum = 626.286133F;
  static constexpr float high_band_minimum = 81.75177F;
  static constexpr float high_band_maximum = 1252.572266F;
  static constexpr float low_resonance = 160.0F;
  static constexpr float high_resonance = 320.0F;
  static constexpr float strength = 2.4F;
  static constexpr float amplitude_ceiling = 1.0F;

  static constexpr auto shape(const SensoryFeedback& feedback,
                              HapticStrength chosen = HapticStrength{}) noexcept
      -> RumbleShape {
    return base(feedback)
        .scaled(strength * chosen.value())
        .clamped(amplitude_ceiling);
  }

 private:
  static constexpr auto base(const SensoryFeedback& feedback) noexcept
      -> RumbleShape {
    using Kind = SensoryFeedback::Kind;
    switch (feedback.kind()) {
      case Kind::selection:
        return selected(feedback);
      case Kind::impact:
        return impact(feedback).scaled(feedback.intensity().value());
      case Kind::increase:
        return {pulse(0.05F, 0.16F, 10, low_resonance, 300.0F), rest(30),
                pulse(0.07F, 0.22F, 10, low_resonance, 360.0F)};
      case Kind::decrease:
        return {pulse(0.07F, 0.22F, 10, low_resonance, 360.0F), rest(30),
                pulse(0.05F, 0.16F, 10, low_resonance, 300.0F)};
      case Kind::success:
        return {pulse(0.18F, 0.24F, 30), rest(60), pulse(0.28F, 0.34F, 40)};
      case Kind::warning:
        return {pulse(0.26F, 0.20F, 40, 140.0F, 280.0F), rest(70),
                pulse(0.26F, 0.20F, 40, 140.0F, 280.0F)};
      case Kind::error:
        return {pulse(0.34F, 0.14F, 30, 110.0F, 220.0F), rest(40),
                pulse(0.34F, 0.14F, 30, 110.0F, 220.0F), rest(40),
                pulse(0.34F, 0.14F, 30, 110.0F, 220.0F)};
      case Kind::start:
        return {pulse(0.08F, 0.12F, 20), pulse(0.18F, 0.26F, 30)};
      case Kind::stop:
        return {pulse(0.18F, 0.26F, 30), pulse(0.08F, 0.12F, 20)};
      case Kind::alignment:
        return {pulse(0.0F, 0.26F, 8, low_resonance, 480.0F)};
      case Kind::level_change:
        return {pulse(0.16F, 0.22F, 18), rest(50),
                pulse(0.16F, 0.22F, 18, low_resonance, 400.0F)};
      case Kind::path_complete:
        return {pulse(0.10F, 0.18F, 14), rest(40),
                pulse(0.14F, 0.24F, 14, low_resonance, 360.0F), rest(40),
                pulse(0.20F, 0.30F, 24, low_resonance, 400.0F)};
      case Kind::press:
        return pressed(feedback);
      case Kind::release:
        return {pulse(0.04F, 0.12F, 8, low_resonance, 260.0F)};
    }
    return {};
  }

  static constexpr auto selected(const SensoryFeedback& feedback) noexcept
      -> RumbleShape {
    using Variant = SensoryFeedback::SelectionFeedback;
    const auto variant = feedback.selection_variant();
    if (!variant.has_value()) {
      return {pulse(0.06F, 0.18F, 10)};
    }
    switch (*variant) {
      case Variant::on:
        return {pulse(0.05F, 0.16F, 8, low_resonance, 360.0F)};
      case Variant::off:
        return {pulse(0.06F, 0.14F, 8, low_resonance, 260.0F)};
      case Variant::minimum:
        return {pulse(0.14F, 0.26F, 12, 180.0F, 400.0F)};
      case Variant::maximum:
        return {pulse(0.16F, 0.28F, 12, 180.0F, 420.0F)};
    }
    return {pulse(0.06F, 0.18F, 10)};
  }

  static constexpr auto pressed(const SensoryFeedback& feedback) noexcept
      -> RumbleShape {
    using Control = SensoryFeedback::PressFeedback;
    switch (feedback.press_feedback().value_or(Control::button)) {
      case Control::button:
        return {pulse(0.08F, 0.22F, 10, low_resonance, 360.0F)};
      case Control::button_icon_only:
        return {pulse(0.06F, 0.18F, 8, low_resonance, 380.0F)};
      case Control::slider:
        return {pulse(0.06F, 0.14F, 10, low_resonance, 300.0F)};
      case Control::toggle:
        return {pulse(0.12F, 0.24F, 12, 180.0F, 360.0F)};
      case Control::tab:
        return {pulse(0.06F, 0.16F, 8)};
    }
    return {pulse(0.08F, 0.22F, 10)};
  }

  static constexpr auto pulse(float low, float high, int milliseconds,
                              float low_hz = low_resonance,
                              float high_hz = high_resonance) noexcept
      -> RumblePulse {
    return RumblePulse{.low_amplitude = low,
                       .low_frequency = low_hz,
                       .high_amplitude = high,
                       .high_frequency = high_hz,
                       .duration = std::chrono::milliseconds{milliseconds}};
  }

  static constexpr auto rest(int milliseconds) noexcept -> RumblePulse {
    return pulse(0.0F, 0.0F, milliseconds);
  }

  static constexpr auto impact(const SensoryFeedback& feedback) noexcept
      -> RumbleShape {
    using Flexibility = SensoryFeedback::Flexibility;
    using Weight = SensoryFeedback::Weight;
    if (const auto flexibility = feedback.flexibility()) {
      switch (*flexibility) {
        case Flexibility::rigid:
          return {pulse(0.10F, 0.34F, 10, 180.0F, 400.0F)};
        case Flexibility::solid:
          return {pulse(0.24F, 0.24F, 18)};
        case Flexibility::soft:
          return {pulse(0.20F, 0.10F, 20, 120.0F, 240.0F),
                  pulse(0.10F, 0.05F, 24, 120.0F, 240.0F)};
      }
    }
    switch (feedback.weight().value_or(Weight::medium)) {
      case Weight::light:
        return {pulse(0.14F, 0.20F, 12)};
      case Weight::medium:
        return {pulse(0.22F, 0.28F, 16)};
      case Weight::heavy:
        return {pulse(0.32F, 0.36F, 22, 140.0F, 280.0F)};
    }
    return {};
  }
};

}
