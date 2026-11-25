#include <arisa/composition/feedback_enablement.hpp>

namespace arisa::composition {

const EnvironmentKey<bool> FeedbackEnablement::is_sensory_feedback_enabled{
    "is_sensory_feedback_enabled", true};

const EnvironmentKey<bool> FeedbackEnablement::is_system_feedback_enabled{
    "is_system_feedback_enabled", true};

const EnvironmentKey<provider::HapticStrength>
    FeedbackEnablement::haptic_strength{"haptic_strength",
                                        provider::HapticStrength{}};

}
