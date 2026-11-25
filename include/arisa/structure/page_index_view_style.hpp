#pragma once

#include <array>
#include <cstdint>

namespace arisa::structure {

class PageIndexViewStyle {
 public:
  enum class BackgroundDisplayMode : std::uint8_t {
    automatic,
    interactive,
    always,
    never,
  };

  static constexpr std::array<BackgroundDisplayMode, 4> all_background_modes{
      BackgroundDisplayMode::automatic, BackgroundDisplayMode::interactive,
      BackgroundDisplayMode::always, BackgroundDisplayMode::never};

  constexpr explicit PageIndexViewStyle(
      BackgroundDisplayMode background_display_mode =
          BackgroundDisplayMode::automatic) noexcept
      : background_display_mode_(background_display_mode) {}

  constexpr auto background_display_mode() const noexcept
      -> BackgroundDisplayMode {
    return background_display_mode_;
  }

  friend constexpr auto operator==(const PageIndexViewStyle&,
                                   const PageIndexViewStyle&) -> bool = default;

 private:
  BackgroundDisplayMode background_display_mode_;
};

}
