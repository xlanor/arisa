#include <arisa/composition/accessibility_settings.hpp>

namespace arisa::composition {

const EnvironmentKey<bool> AccessibilitySettings::differentiate_without_color{
    "accessibility_differentiate_without_color", false};

const EnvironmentKey<bool> AccessibilitySettings::show_button_shapes{
    "accessibility_show_button_shapes", false};

const EnvironmentKey<bool> AccessibilitySettings::dim_flashing_lights{
    "accessibility_dim_flashing_lights", false};

const EnvironmentKey<AccessibilityTechnologies>
    AccessibilitySettings::technologies{"accessibility_technologies",
                                        AccessibilityTechnologies{}};

const EnvironmentKey<bool> AccessibilitySettings::enabled{
    "accessibility_enabled", false};

const EnvironmentKey<bool> AccessibilitySettings::voice_over_enabled{
    "accessibility_voice_over_enabled", false};

const EnvironmentKey<bool> AccessibilitySettings::switch_control_enabled{
    "accessibility_switch_control_enabled", false};

const EnvironmentKey<bool> AccessibilitySettings::invert_colors{
    "accessibility_invert_colors", false};

const EnvironmentKey<bool> AccessibilitySettings::play_animated_images{
    "accessibility_play_animated_images", true};

const EnvironmentKey<bool>
    AccessibilitySettings::prefers_cross_fade_transitions{
        "accessibility_prefers_cross_fade_transitions", false};

const EnvironmentKey<bool> AccessibilitySettings::reduce_highlighting_effects{
    "accessibility_reduce_highlighting_effects", false};

const EnvironmentKey<bool> AccessibilitySettings::show_borders{
    "accessibility_show_borders", false};

const EnvironmentKey<bool> AccessibilitySettings::assistive_access_enabled{
    "accessibility_assistive_access_enabled", false};

const EnvironmentKey<bool> AccessibilitySettings::large_content_viewer_enabled{
    "accessibility_large_content_viewer_enabled", false};

}
