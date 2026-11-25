#pragma once

#include <arisa/provider/speech/provider.hpp>

#include <cstddef>
#include <mutex>
#include <vector>

namespace arisa::provider::headless {

class RecordingSpeech final : public SpeechProvider {
 public:
  [[nodiscard]] auto speak(const Utterance& utterance)
      -> std::expected<void, ProviderError> override;

  void stop() override;

  auto spoken() const -> std::vector<Utterance>;

  auto stops() const -> std::size_t;

 private:
  mutable std::mutex mutex_;
  std::vector<Utterance> spoken_;
  std::size_t stops_{0};
};

}
