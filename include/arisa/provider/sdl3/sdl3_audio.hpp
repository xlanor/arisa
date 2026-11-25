#pragma once

#include <arisa/provider/assets/provider.hpp>
#include <arisa/provider/audio/app_voice.hpp>
#include <arisa/provider/audio/provider.hpp>
#include <arisa/provider/audio/sound_bank.hpp>
#include <arisa/provider/diagnostics/provider.hpp>
#include <arisa/provider/sdl3/sdl3_context.hpp>
#include <arisa/provider/threading/provider.hpp>

#include <expected>
#include <functional>
#include <memory>
#include <optional>

namespace arisa::provider::sdl3 {

class Sdl3Audio final : public AudioProvider {
 public:
  using Threading = std::optional<std::reference_wrapper<ThreadingProvider>>;

  [[nodiscard]] static auto create(const AssetProvider& assets,
                                   DiagnosticsProvider& diagnostics,
                                   const SoundBank& sounds = {},
                                   Threading threading = std::nullopt)
      -> std::expected<Sdl3Audio, ProviderError>;

  Sdl3Audio(const Sdl3Audio&) = delete;
  auto operator=(const Sdl3Audio&) -> Sdl3Audio& = delete;
  Sdl3Audio(Sdl3Audio&& other) noexcept;
  auto operator=(Sdl3Audio&& other) noexcept -> Sdl3Audio&;
  ~Sdl3Audio() override;

  [[nodiscard]] auto play(SoundRole role)
      -> std::expected<void, ProviderError> override;

  [[nodiscard]] auto open_voice(AppVoiceOptions options)
      -> std::expected<AppVoice, ProviderError> override;

  auto ui_gain() const noexcept -> float;

 private:
  class Mixer;
  class Voice;

  Sdl3Audio(Sdl3Context audio, std::shared_ptr<Mixer> mixer,
            Threading threading) noexcept;

  Sdl3Context audio_;
  std::shared_ptr<Mixer> mixer_;
  Threading threading_;
};

}
