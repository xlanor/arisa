#include <arisa/provider/audio/absent.hpp>

namespace arisa::provider {

auto AbsentAudioProvider::play(SoundRole)
    -> std::expected<void, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

}
