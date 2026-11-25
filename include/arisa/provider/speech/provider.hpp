#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/speech/utterance.hpp>

#include <expected>

namespace arisa::provider {

class SpeechProvider {
 public:
  virtual ~SpeechProvider() = default;

  [[nodiscard]] virtual auto speak(const Utterance& utterance)
      -> std::expected<void, ProviderError> = 0;

  virtual void stop() = 0;
};

}
