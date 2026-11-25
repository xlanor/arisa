#pragma once

#include <arisa/appearance/appearance_settings.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/theme/colour.hpp>

#include <array>
#include <cstddef>
#include <flat_map>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::structure {

class AppearanceSettingsSection final : public composition::Component {
 public:
  static constexpr bool is_section = true;

  struct Accent {
    std::string_view name;
    theme::Colour colour;
  };

  static constexpr std::array accents{
      Accent{"Blue", theme::Colour::hex(0x007AFF)},
      Accent{"Purple", theme::Colour::hex(0xAF52DE)},
      Accent{"Pink", theme::Colour::hex(0xFF2D55)},
      Accent{"Red", theme::Colour::hex(0xFF3B30)},
      Accent{"Orange", theme::Colour::hex(0xFF9500)},
      Accent{"Yellow", theme::Colour::hex(0xFFCC00)},
      Accent{"Green", theme::Colour::hex(0x28CD41)},
      Accent{"Graphite", theme::Colour::hex(0x8E8E93)}};

  struct Labels {
    std::string header{"Appearance"};
    std::string unavailable{"Appearance settings are unavailable"};
    std::string theme{"Theme"};
    std::string appearance{"Appearance"};
    std::string system{"System"};
    std::string light{"Light"};
    std::string dark{"Dark"};
    std::string accent{"Accent"};
    std::string theme_accent{"Theme"};
    std::array<std::string, 8> accents{"Blue",   "Purple", "Pink",  "Red",
                                       "Orange", "Yellow", "Green", "Graphite"};
    std::string text_size{"Text size: {}"};
    std::array<std::string, 12> sizes{
        "Extra Small", "Small", "Medium", "Large", "Extra Large", "XX Large",
        "XXX Large",   "AX1",   "AX2",    "AX3",   "AX4",         "AX5"};
    std::string interface_scale{"Interface scale: {}%"};
    std::string reduce_motion{"Reduce motion"};
    std::string reduce_transparency{"Reduce transparency"};
    std::string increase_contrast{"Increase contrast"};
    std::string bold_text{"Bold text"};
    std::string button_shapes{"Button shapes"};
    std::string differentiate_without_colour{"Differentiate without colour"};
    std::string dim_flashing_lights{"Dim flashing lights"};
    std::string sensory_feedback{"Sensory feedback"};
    std::string haptic_strength{"Haptic strength"};
    std::flat_map<std::string, std::string> theme_names{};

    friend auto operator==(const Labels&, const Labels&) -> bool = default;
  };

  static constexpr std::string_view header = "Appearance";
  static constexpr std::size_t theme_accent = accents.size();
  static constexpr float scale_step = 0.05F;

  AppearanceSettingsSection() = default;

  explicit AppearanceSettingsSection(Labels labels)
      : labels_(std::move(labels)) {}

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto size_label(int index) -> std::string_view;

  auto labels() const noexcept -> const Labels& { return labels_; }

  static auto accent_index(const appearance::AppearanceSettings& settings)
      -> std::size_t;

  friend auto operator==(const AppearanceSettingsSection&,
                         const AppearanceSettingsSection&) -> bool = default;

 private:
  auto sized(int index) const -> const std::string&;

  static auto filled(const std::string& pattern, std::string_view value)
      -> std::string;

  Labels labels_;
};

}
