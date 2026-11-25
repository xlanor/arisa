#pragma once

#include <arisa/provider/audio/provider.hpp>

namespace arisa::provider {

class AbsentAudioProvider final : public AudioProvider {
 public:
  [[nodiscard]] auto play(SoundRole role)
      -> std::expected<void, ProviderError> override;
};

}
