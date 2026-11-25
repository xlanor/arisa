#include <arisa/composition/control_environment.hpp>

namespace arisa::composition {

const EnvironmentKey<Visibility> ControlEnvironment::labels_visibility{
    "labels_visibility", Visibility::automatic};
const EnvironmentKey<Visibility> ControlEnvironment::menu_indicator{
    "menu_indicator", Visibility::automatic};
const EnvironmentKey<MenuOrder> ControlEnvironment::menu_order{
    "menu_order", MenuOrder::automatic};
const EnvironmentKey<MenuActionDismissBehavior>
    ControlEnvironment::menu_action_dismiss_behavior{
        "menu_action_dismiss_behavior", MenuActionDismissBehavior::automatic};
const EnvironmentKey<bool> ControlEnvironment::menu_opens_upward{
    "menu_opens_upward", false};
const EnvironmentKey<std::optional<float>>
    ControlEnvironment::label_icon_to_title_spacing{
        "label_icon_to_title_spacing", std::nullopt};
const EnvironmentKey<std::optional<float>>
    ControlEnvironment::label_reserved_icon_width{"label_reserved_icon_width",
                                                  std::nullopt};
const EnvironmentKey<Visibility> ControlEnvironment::slider_thumb_visibility{
    "slider_thumb_visibility", Visibility::automatic};
const EnvironmentKey<Axis> ControlEnvironment::radio_group_layout{
    "radio_group_layout", Axis::vertical};

}
