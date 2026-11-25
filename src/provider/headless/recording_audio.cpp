#include <arisa/provider/headless/recording_audio.hpp>

namespace arisa::provider::headless {

auto RecordingAudio::play(SoundRole role)
    -> std::expected<void, ProviderError> {
  const std::scoped_lock lock{mutex_};
  played_.push_back(role);
  return {};
}

auto RecordingAudio::played() const -> std::vector<SoundRole> {
  const std::scoped_lock lock{mutex_};
  return played_;
}

}
