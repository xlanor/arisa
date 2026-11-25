#pragma once

#include <arisa/appearance/appearance_settings.hpp>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace arisa::appearance {

enum class AppearanceField : std::uint32_t {
  theme = 1U << 0U,
  color_scheme = 1U << 1U,
  accent = 1U << 2U,
  dynamic_type_size = 1U << 3U,
  ui_scale = 1U << 4U,
  reduce_motion = 1U << 5U,
  reduce_transparency = 1U << 6U,
  increase_contrast = 1U << 7U,
  sidebar = 1U << 8U,
  sensory_feedback = 1U << 9U,
  haptic_strength = 1U << 10U,
  bold_text = 1U << 11U,
  button_shapes = 1U << 12U,
  differentiate_without_color = 1U << 13U,
  dim_flashing_lights = 1U << 14U,
  voice_over = 1U << 15U,
  switch_control = 1U << 16U,
  invert_colors = 1U << 17U,
  play_animated_images = 1U << 18U,
  prefers_cross_fade_transitions = 1U << 19U,
  reduce_highlighting_effects = 1U << 20U,
  show_borders = 1U << 21U,
  assistive_access = 1U << 22U,
};

class AppearanceLocks {
 public:
  constexpr AppearanceLocks() noexcept = default;

  constexpr AppearanceLocks(
      std::initializer_list<AppearanceField> fields) noexcept {
    for (const AppearanceField field : fields) {
      bits_ |= std::to_underlying(field);
    }
  }

  constexpr auto contains(AppearanceField field) const noexcept -> bool {
    return (bits_ & std::to_underlying(field)) != 0U;
  }

  friend constexpr auto operator==(AppearanceLocks, AppearanceLocks)
      -> bool = default;

 private:
  std::uint32_t bits_{0};
};

struct AppearancePolicy {
  std::string application{"arisa"};
  AppearanceSettings defaults{};
  AppearanceLocks locks{};
  std::vector<std::string> packs{};

  auto enforce(AppearanceSettings wanted) const -> AppearanceSettings;

  friend auto operator==(const AppearancePolicy&, const AppearancePolicy&)
      -> bool = default;
};

}
