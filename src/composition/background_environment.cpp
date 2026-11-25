#include <arisa/composition/background_environment.hpp>

namespace arisa::composition {

const EnvironmentKey<std::optional<graphics::ShapeStyle>>
    BackgroundEnvironment::style{"background-style", std::nullopt};

const EnvironmentKey<std::optional<theme::Material>>
    BackgroundEnvironment::material{"background-material", std::nullopt};

const EnvironmentKey<BackgroundProminence> BackgroundEnvironment::prominence{
    "background-prominence", BackgroundProminence::standard};

const EnvironmentKey<bool>
    BackgroundEnvironment::content_transition_adds_drawing_group{
        "content-transition-adds-drawing-group", false};

const EnvironmentKey<motion::ContentTransition>
    BackgroundEnvironment::content_transition{
        "content-transition", motion::ContentTransition::identity};

}
