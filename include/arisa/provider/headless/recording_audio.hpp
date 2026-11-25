#pragma once

#include <arisa/provider/audio/provider.hpp>

#include <mutex>
#include <vector>

namespace arisa::provider::headless {

class RecordingAudio final : public AudioProvider {
 public:
  [[nodiscard]] auto play(SoundRole role)
      -> std::expected<void, ProviderError> override;

  auto played() const -> std::vector<SoundRole>;

 private:
  mutable std::mutex mutex_;
  std::vector<SoundRole> played_;
};

}
