#include <arisa/provider/haptics/absent.hpp>

namespace arisa::provider {

auto AbsentHapticsProvider::play(const SensoryFeedback&, HapticStrength)
    -> std::expected<void, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

}
