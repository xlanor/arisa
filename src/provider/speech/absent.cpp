#include <arisa/provider/speech/absent.hpp>

namespace arisa::provider {

auto AbsentSpeechProvider::speak(const Utterance&)
    -> std::expected<void, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

}
