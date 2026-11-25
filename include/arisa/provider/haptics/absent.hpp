#pragma once

#include <arisa/provider/haptics/provider.hpp>

namespace arisa::provider {

class AbsentHapticsProvider final : public HapticsProvider {
 public:
  [[nodiscard]] auto play(const SensoryFeedback& feedback,
                          HapticStrength strength)
      -> std::expected<void, ProviderError> override;
};

}
