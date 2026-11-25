#pragma once

#include <arisa/provider/applets/absent.hpp>
#include <arisa/provider/audio/absent.hpp>
#include <arisa/provider/audio/sound_bank.hpp>
#include <arisa/provider/browser/absent.hpp>
#include <arisa/provider/clipboard/absent.hpp>
#include <arisa/provider/factory.hpp>
#include <arisa/provider/fonts/absent.hpp>
#include <arisa/provider/gpu/absent.hpp>
#include <arisa/provider/haptics/absent.hpp>
#include <arisa/provider/horizon/console.hpp>
#include <arisa/provider/horizon/gpu_options.hpp>
#include <arisa/provider/horizon/horizon_applets.hpp>
#include <arisa/provider/horizon/horizon_audio.hpp>
#include <arisa/provider/horizon/horizon_browser.hpp>
#include <arisa/provider/horizon/horizon_clock.hpp>
#include <arisa/provider/horizon/horizon_fonts.hpp>
#include <arisa/provider/horizon/horizon_haptics.hpp>
#include <arisa/provider/horizon/horizon_input.hpp>
#include <arisa/provider/horizon/horizon_lifecycle.hpp>
#include <arisa/provider/horizon/horizon_surface.hpp>
#include <arisa/provider/horizon/horizon_system_info.hpp>
#include <arisa/provider/horizon/horizon_text_input.hpp>
#include <arisa/provider/horizon/horizon_threading.hpp>
#include <arisa/provider/horizon/service_session.hpp>
#include <arisa/provider/input/absent.hpp>
#include <arisa/provider/lifecycle/absent.hpp>
#include <arisa/provider/providers.hpp>
#include <arisa/provider/standard/directory_assets.hpp>
#include <arisa/provider/standard/stderr_diagnostics.hpp>
#include <arisa/provider/surface/absent.hpp>
#include <arisa/provider/system_info/absent.hpp>
#include <arisa/provider/text_input/absent.hpp>

#include <switch.h>

#include <atomic>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace arisa::provider::horizon {

class HorizonProviderFactory final : public ProviderFactory {
 public:
  static constexpr std::string_view romfs_root = "romfs:/";

  [[nodiscard]] static auto create(Providers overrides = {},
                                   const SoundBank& sounds = {},
                                   GpuOptions gpu = {})
      -> std::expected<HorizonProviderFactory, ProviderError>;

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
  auto applets() noexcept -> AppletProvider& override;
  auto fonts() noexcept -> FontProvider& override;
  auto surface() noexcept -> SurfaceProvider& override;
  auto input() noexcept -> InputProvider& override;
  auto gpu() noexcept -> GpuProvider& override;
  auto text_input() noexcept -> TextInputProvider& override;

  static constexpr auto default_gpu_api() noexcept -> GpuApi {
#if defined(ARISA_HORIZON_VULKAN)
    return GpuApi::vulkan;
#elif defined(__SWITCH2__)
    return GpuApi::drako3d;
#else
    return GpuApi::deko3d;
#endif
  }

  static constexpr auto supports(GpuApi api) noexcept -> bool {
    switch (api) {
      case GpuApi::deko3d:
#if defined(ARISA_HORIZON_DEKO3D) && !defined(__SWITCH2__)
        return true;
#else
        return false;
#endif
      case GpuApi::drako3d:
#if defined(ARISA_HORIZON_DEKO3D) && defined(__SWITCH2__)
        return true;
#else
        return false;
#endif
      case GpuApi::vulkan:
#if defined(ARISA_HORIZON_VULKAN)
        return true;
#else
        return false;
#endif
    }
    return false;
  }

  auto gpu_api() const noexcept -> std::optional<GpuApi> { return gpu_api_; }

  template <class Gpu>
  auto stock_gpu() noexcept -> std::optional<std::reference_wrapper<Gpu>> {
    if (overrides_.gpu.has_value()) {
      return std::nullopt;
    }
#if defined(ARISA_HORIZON_DEKO3D)
    if constexpr (std::is_same_v<Gpu, Deko3dGpu>) {
      if (deko3d_.has_value()) {
        return std::ref(*deko3d_);
      }
    }
#endif
#if defined(ARISA_HORIZON_VULKAN)
    if constexpr (std::is_same_v<Gpu, vulkan::VulkanGpu>) {
      if (vulkan_.has_value()) {
        return std::ref(*vulkan_);
      }
    }
#endif
    return std::nullopt;
  }

  auto stock_gpu_report() -> std::optional<std::string>;

 private:
  struct RomfsService {
    static auto open() noexcept -> Result { return romfsMountSelf("romfs"); }

    static void close() noexcept { static_cast<void>(romfsUnmount("romfs")); }
  };

  HorizonProviderFactory(Providers overrides,
                         std::optional<ServiceSession<RomfsService>> romfs,
                         std::optional<HorizonClock> clock);

  [[nodiscard]] static auto mount_romfs()
      -> std::expected<std::optional<ServiceSession<RomfsService>>,
                       ProviderError>;

  template <class Stock>
  void adopt(std::optional<Stock>& slot,
             std::expected<Stock, ProviderError> created,
             std::string_view capability) {
    if (created.has_value()) {
      slot.emplace(std::move(*created));
      return;
    }
    report_unavailable(capability, created.error());
  }

  void report_unavailable(std::string_view capability,
                          const ProviderError& error);

  void open_gpu(const GpuOptions& options);

  template <class Provider, class Stock>
  static auto select(
      const std::optional<std::reference_wrapper<Provider>>& chosen,
      std::optional<Stock>& stock,
      std::type_identity_t<Provider>& absent) noexcept -> Provider& {
    if (chosen.has_value()) {
      return chosen->get();
    }
    if (stock.has_value()) {
      return *stock;
    }
    return absent;
  }

  template <class Provider>
  static auto supplied(
      const std::optional<std::reference_wrapper<Provider>>& chosen,
      bool stock) noexcept -> bool {
    return chosen.has_value() || stock;
  }

  Providers overrides_;
  std::optional<ServiceSession<RomfsService>> romfs_;
  HorizonThreadingProvider threading_;
  std::optional<HorizonClock> clock_;
  std::unique_ptr<standard::StderrDiagnostics> diagnostics_;
  standard::DirectoryAssets assets_;
  std::optional<HorizonLifecycle> lifecycle_;
  std::optional<HorizonSystemInfo> system_info_;
  std::optional<HorizonHaptics> haptics_;
  std::optional<HorizonAudio> audio_;
  std::optional<HorizonBrowser> browser_;
  std::optional<HorizonApplets> applets_;
  std::optional<HorizonFonts> fonts_;
  std::unique_ptr<HorizonSurface> surface_;
  std::unique_ptr<AbsentSurfaceProvider> absent_surface_;
  std::shared_ptr<std::atomic<bool>> keyboard_shown_{
      std::make_shared<std::atomic<bool>>(false)};
  std::optional<HorizonInput> input_;
#if defined(ARISA_HORIZON_DEKO3D)
  std::optional<Deko3dGpu> deko3d_;
#endif
#if defined(ARISA_HORIZON_VULKAN)
  std::optional<vulkan::VulkanGpu> vulkan_;
#endif
  std::optional<GpuApi> gpu_api_;
  std::optional<HorizonTextInput> text_input_;
  AbsentLifecycleProvider absent_lifecycle_;
  AbsentSystemInfoProvider absent_system_info_;
  AbsentHapticsProvider absent_haptics_;
  AbsentAudioProvider absent_audio_;
  AbsentClipboardProvider absent_clipboard_;
  AbsentBrowserProvider absent_browser_;
  AbsentAppletProvider absent_applets_;
  AbsentFontProvider absent_fonts_;
  AbsentInputProvider absent_input_;
  AbsentGpuProvider absent_gpu_;
  AbsentTextInputProvider absent_text_input_;
};

}
