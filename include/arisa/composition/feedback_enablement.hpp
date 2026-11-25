#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/provider/haptics/haptic_strength.hpp>

namespace arisa::composition {

class FeedbackEnablement {
 public:
  static const EnvironmentKey<bool> is_sensory_feedback_enabled;
  static const EnvironmentKey<bool> is_system_feedback_enabled;
  static const EnvironmentKey<provider::HapticStrength> haptic_strength;
};

}
