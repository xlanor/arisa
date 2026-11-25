#include <arisa/composition/spring_loading_behavior.hpp>

namespace arisa::composition {

const EnvironmentKey<SpringLoadingBehavior> SpringLoading::behavior{
    "spring_loading_behavior", SpringLoadingBehavior::automatic};

}
