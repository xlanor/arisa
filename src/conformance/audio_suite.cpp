#include <arisa/conformance/audio_suite.hpp>

#include <algorithm>

namespace arisa::conformance {

using provider::ProviderErrorCode;

void AudioSuite::run(provider::ProviderFactory& factory, Report& report) {
  check_roles(factory.audio(), factory.supplies(provider::Capability::audio),
              report);
}

void AudioSuite::check_roles(provider::AudioProvider& audio, bool supplied,
                             Report& report) {
  const bool conforms = std::ranges::all_of(
      provider::all_sound_roles, [&audio, supplied](provider::SoundRole role) {
        const auto played = audio.play(role);
        const bool unsupported =
            !played.has_value() &&
            played.error().code() == ProviderErrorCode::unsupported_capability;
        return supplied != unsupported;
      });
  report.record(supplied ? "audio.play" : "audio.absent", conforms,
                supplied ? "play() reported unsupported_capability while "
                           "supplied"
                         : "an unsupplied audio provider did not return "
                           "unsupported_capability");
}

}
