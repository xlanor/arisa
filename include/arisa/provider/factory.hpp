#pragma once

#include <arisa/provider/applets/provider.hpp>
#include <arisa/provider/assets/provider.hpp>
#include <arisa/provider/audio/provider.hpp>
#include <arisa/provider/browser/provider.hpp>
#include <arisa/provider/capability.hpp>
#include <arisa/provider/clipboard/provider.hpp>
#include <arisa/provider/clock/provider.hpp>
#include <arisa/provider/diagnostics/provider.hpp>
#include <arisa/provider/fonts/provider.hpp>
#include <arisa/provider/gpu/provider.hpp>
#include <arisa/provider/haptics/provider.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/lifecycle/provider.hpp>
#include <arisa/provider/provider_info.hpp>
#include <arisa/provider/speech/provider.hpp>
#include <arisa/provider/surface/provider.hpp>
#include <arisa/provider/system_info/provider.hpp>
#include <arisa/provider/text_input/provider.hpp>
#include <arisa/provider/threading/provider.hpp>

namespace arisa::provider {

class ProviderFactory {
 public:
  virtual ~ProviderFactory() = default;

  virtual auto info() const -> ProviderInfo = 0;

  auto supplies(Capability capability) const -> bool;

  virtual auto threading() noexcept -> ThreadingProvider& = 0;
  virtual auto clock() noexcept -> ClockProvider& = 0;
  virtual auto diagnostics() noexcept -> DiagnosticsProvider& = 0;
  virtual auto assets() noexcept -> AssetProvider& = 0;
  virtual auto lifecycle() noexcept -> LifecycleProvider& = 0;
  virtual auto system_info() noexcept -> SystemInfoProvider& = 0;
  virtual auto haptics() noexcept -> HapticsProvider& = 0;
  virtual auto audio() noexcept -> AudioProvider& = 0;
  virtual auto clipboard() noexcept -> ClipboardProvider& = 0;
  virtual auto browser() noexcept -> BrowserProvider& = 0;
  virtual auto fonts() noexcept -> FontProvider& = 0;
  virtual auto surface() noexcept -> SurfaceProvider& = 0;
  virtual auto input() noexcept -> InputProvider& = 0;
  virtual auto gpu() noexcept -> GpuProvider& = 0;
  virtual auto text_input() noexcept -> TextInputProvider& = 0;
  virtual auto applets() noexcept -> AppletProvider&;
  virtual auto speech() noexcept -> SpeechProvider&;
};

}
