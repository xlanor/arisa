#pragma once

#include <arisa/provider/assets/provider.hpp>
#include <arisa/provider/audio/provider.hpp>
#include <arisa/provider/audio/sound_bank.hpp>
#include <arisa/provider/diagnostics/provider.hpp>
#include <arisa/provider/threading/provider.hpp>

#include <functional>
#include <memory>
#include <optional>

namespace arisa::provider::horizon {

class HorizonAudio final : public AudioProvider {
 public:
  using Threading = std::optional<std::reference_wrapper<ThreadingProvider>>;

  [[nodiscard]] static auto create(const AssetProvider& assets,
                                   DiagnosticsProvider& diagnostics,
                                   const SoundBank& sounds = {},
                                   Threading threading = std::nullopt)
      -> std::expected<HorizonAudio, ProviderError>;

  HorizonAudio(const HorizonAudio&) = delete;
  auto operator=(const HorizonAudio&) -> HorizonAudio& = delete;
  HorizonAudio(HorizonAudio&& other) noexcept;
  auto operator=(HorizonAudio&& other) noexcept -> HorizonAudio&;
  ~HorizonAudio() override;

  [[nodiscard]] auto play(SoundRole role)
      -> std::expected<void, ProviderError> override;

  [[nodiscard]] auto open_voice(AppVoiceOptions options)
      -> std::expected<AppVoice, ProviderError> override;

 private:
  class Renderer;
  class Voice;

  HorizonAudio(std::shared_ptr<Renderer> renderer,
               Threading threading) noexcept;

  std::shared_ptr<Renderer> renderer_;
  Threading threading_;
};

}
