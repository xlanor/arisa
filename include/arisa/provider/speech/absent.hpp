#pragma once

#include <arisa/provider/speech/provider.hpp>

namespace arisa::provider {

class AbsentSpeechProvider final : public SpeechProvider {
 public:
  [[nodiscard]] auto speak(const Utterance& utterance)
      -> std::expected<void, ProviderError> override;

  void stop() override {}
};

}
