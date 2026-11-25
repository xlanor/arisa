#include <arisa/provider/headless/recording_speech.hpp>

namespace arisa::provider::headless {

auto RecordingSpeech::speak(const Utterance& utterance)
    -> std::expected<void, ProviderError> {
  const std::scoped_lock lock{mutex_};
  spoken_.push_back(utterance);
  return {};
}

void RecordingSpeech::stop() {
  const std::scoped_lock lock{mutex_};
  ++stops_;
}

auto RecordingSpeech::spoken() const -> std::vector<Utterance> {
  const std::scoped_lock lock{mutex_};
  return spoken_;
}

auto RecordingSpeech::stops() const -> std::size_t {
  const std::scoped_lock lock{mutex_};
  return stops_;
}

}
