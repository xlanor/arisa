#include <arisa/components/primitive_button_style.hpp>

namespace arisa::components {

const composition::EnvironmentKey<std::optional<PrimitiveButtonStyle>>
    PrimitiveButtonStyle::key{"primitive_button_style", std::nullopt};

}
