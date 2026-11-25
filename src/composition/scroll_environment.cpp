#include <arisa/composition/scroll_environment.hpp>

namespace arisa::composition {

const EnvironmentKey<Visibility> ScrollEnvironment::content_background{
    "scroll_content_background", Visibility::automatic};

const EnvironmentKey<bool> ScrollEnvironment::is_scroll_enabled{
    "is_scroll_enabled", true};

const EnvironmentKey<ScrollIndicatorVisibility>
    ScrollEnvironment::horizontal_scroll_indicator_visibility{
        "horizontal_scroll_indicator_visibility",
        ScrollIndicatorVisibility::automatic};

const EnvironmentKey<ScrollIndicatorVisibility>
    ScrollEnvironment::vertical_scroll_indicator_visibility{
        "vertical_scroll_indicator_visibility",
        ScrollIndicatorVisibility::automatic};

const EnvironmentKey<ScrollBounceBehavior>
    ScrollEnvironment::horizontal_scroll_bounce_behavior{
        "horizontal_scroll_bounce_behavior", ScrollBounceBehavior::automatic};

const EnvironmentKey<ScrollBounceBehavior>
    ScrollEnvironment::vertical_scroll_bounce_behavior{
        "vertical_scroll_bounce_behavior", ScrollBounceBehavior::automatic};

const EnvironmentKey<ScrollDismissesKeyboardMode>
    ScrollEnvironment::scroll_dismisses_keyboard_mode{
        "scroll_dismisses_keyboard_mode",
        ScrollDismissesKeyboardMode::automatic};

const EnvironmentKey<ScrollEdgeEffects> ScrollEnvironment::scroll_edge_effects{
    "scroll_edge_effects", ScrollEdgeEffects{}, &ScrollEdgeEffects::merged};

}
