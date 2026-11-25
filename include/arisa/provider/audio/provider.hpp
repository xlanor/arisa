#pragma once

#include <arisa/provider/audio/app_voice.hpp>
#include <arisa/provider/error.hpp>

#include <array>
#include <cstdint>
#include <expected>

namespace arisa::provider {

enum class SoundRole : std::uint8_t {
  focus_change,
  focus_error,
  click,
  back,
  error,
  slider_tick,
  slider_release,
  touch,
  sidebar,
};

inline constexpr std::array all_sound_roles{
    SoundRole::focus_change,   SoundRole::focus_error, SoundRole::click,
    SoundRole::back,           SoundRole::error,       SoundRole::slider_tick,
    SoundRole::slider_release, SoundRole::touch,       SoundRole::sidebar};

class AudioProvider {
 public:
  virtual ~AudioProvider() = default;

  [[nodiscard]] virtual auto play(SoundRole role)
      -> std::expected<void, ProviderError> = 0;

  [[nodiscard]] virtual auto open_voice(AppVoiceOptions)
      -> std::expected<AppVoice, ProviderError> {
    return std::unexpected(ProviderError::unsupported_capability());
  }
};

}
