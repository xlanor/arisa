#pragma once

#include <arisa/provider/audio/absent.hpp>
#include <arisa/provider/browser/absent.hpp>
#include <arisa/provider/clipboard/absent.hpp>
#include <arisa/provider/factory.hpp>
#include <arisa/provider/fonts/absent.hpp>
#include <arisa/provider/gpu/absent.hpp>
#include <arisa/provider/haptics/absent.hpp>
#include <arisa/provider/input/absent.hpp>
#include <arisa/provider/lifecycle/absent.hpp>
#include <arisa/provider/providers.hpp>
#include <arisa/provider/standard/directory_assets.hpp>
#include <arisa/provider/standard/standard_clock.hpp>
#include <arisa/provider/standard/std_threading.hpp>
#include <arisa/provider/standard/stderr_diagnostics.hpp>
#include <arisa/provider/surface/absent.hpp>
#include <arisa/provider/system_info/absent.hpp>
#include <arisa/provider/text_input/absent.hpp>

#include <functional>
#include <optional>
#include <type_traits>

namespace arisa::provider {

class DefaultProviderFactory final : public ProviderFactory {
 public:
  explicit DefaultProviderFactory(Providers overrides = {});

  auto info() const -> ProviderInfo override;

  auto threading() noexcept -> ThreadingProvider& override;
  auto clock() noexcept -> ClockProvider& override;
  auto diagnostics() noexcept -> DiagnosticsProvider& override;
  auto assets() noexcept -> AssetProvider& override;
  auto lifecycle() noexcept -> LifecycleProvider& override;
  auto system_info() noexcept -> SystemInfoProvider& override;
  auto haptics() noexcept -> HapticsProvider& override;
  auto audio() noexcept -> AudioProvider& override;
  auto clipboard() noexcept -> ClipboardProvider& override;
  auto browser() noexcept -> BrowserProvider& override;
  auto fonts() noexcept -> FontProvider& override;
  auto surface() noexcept -> SurfaceProvider& override;
  auto input() noexcept -> InputProvider& override;
  auto gpu() noexcept -> GpuProvider& override;
  auto text_input() noexcept -> TextInputProvider& override;

 private:
  template <class Provider>
  static auto select(
      const std::optional<std::reference_wrapper<Provider>>& chosen,
      std::type_identity_t<Provider>& fallback) noexcept -> Provider& {
    return chosen.has_value() ? chosen->get() : fallback;
  }

  Providers overrides_;
  standard::StdThreadingProvider threading_;
  standard::StandardClock clock_;
  standard::StderrDiagnostics diagnostics_;
  standard::DirectoryAssets assets_;
  AbsentLifecycleProvider lifecycle_;
  AbsentSystemInfoProvider system_info_;
  AbsentHapticsProvider haptics_;
  AbsentAudioProvider audio_;
  AbsentClipboardProvider clipboard_;
  AbsentBrowserProvider browser_;
  AbsentFontProvider fonts_;
  AbsentSurfaceProvider surface_;
  AbsentInputProvider input_;
  AbsentGpuProvider gpu_;
  AbsentTextInputProvider text_input_;
};

}
