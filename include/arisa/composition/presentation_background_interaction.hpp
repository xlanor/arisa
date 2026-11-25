#pragma once

#include <arisa/composition/presentation_detent.hpp>

#include <cstdint>
#include <optional>

namespace arisa::composition {

class PresentationBackgroundInteraction {
 public:
  static const PresentationBackgroundInteraction automatic;
  static const PresentationBackgroundInteraction enabled;
  static const PresentationBackgroundInteraction disabled;

  static constexpr auto enabled_up_through(PresentationDetent detent) noexcept
      -> PresentationBackgroundInteraction {
    return PresentationBackgroundInteraction{Mode::enabled, detent};
  }

  constexpr auto allows(float shown, float largest) const noexcept -> bool {
    if (mode_ != Mode::enabled) {
      return false;
    }
    return !through_.has_value() || shown <= through_->resolve(largest) + 0.5F;
  }

  constexpr auto is_enabled() const noexcept -> bool {
    return mode_ == Mode::enabled;
  }

  friend constexpr auto operator==(const PresentationBackgroundInteraction&,
                                   const PresentationBackgroundInteraction&)
      -> bool = default;

 private:
  enum class Mode : std::uint8_t {
    automatic,
    enabled,
    disabled,
  };

  constexpr explicit PresentationBackgroundInteraction(
      Mode mode, std::optional<PresentationDetent> through = {}) noexcept
      : mode_(mode), through_(through) {}

  Mode mode_;
  std::optional<PresentationDetent> through_;
};

inline constexpr PresentationBackgroundInteraction
    PresentationBackgroundInteraction::automatic{
        PresentationBackgroundInteraction::Mode::automatic};
inline constexpr PresentationBackgroundInteraction
    PresentationBackgroundInteraction::enabled{
        PresentationBackgroundInteraction::Mode::enabled};
inline constexpr PresentationBackgroundInteraction
    PresentationBackgroundInteraction::disabled{
        PresentationBackgroundInteraction::Mode::disabled};

}
