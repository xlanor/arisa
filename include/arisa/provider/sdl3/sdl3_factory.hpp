#pragma once

#include <arisa/provider/audio/absent.hpp>
#include <arisa/provider/audio/sound_bank.hpp>
#include <arisa/provider/browser/absent.hpp>
#include <arisa/provider/clipboard/absent.hpp>
#include <arisa/provider/factory.hpp>
#include <arisa/provider/fonts/absent.hpp>
#include <arisa/provider/fonts/font_bank.hpp>
#include <arisa/provider/gpu/absent.hpp>
#include <arisa/provider/haptics/absent.hpp>
#include <arisa/provider/input/absent.hpp>
#include <arisa/provider/lifecycle/absent.hpp>
#include <arisa/provider/providers.hpp>
#include <arisa/provider/sdl3/sdl3_audio.hpp>
#include <arisa/provider/sdl3/sdl3_browser.hpp>
#include <arisa/provider/sdl3/sdl3_clipboard.hpp>
#include <arisa/provider/sdl3/sdl3_event_pump.hpp>
#include <arisa/provider/sdl3/sdl3_fonts.hpp>
#include <arisa/provider/sdl3/sdl3_gpu.hpp>
#include <arisa/provider/sdl3/sdl3_haptics.hpp>
#include <arisa/provider/sdl3/sdl3_input.hpp>
#include <arisa/provider/sdl3/sdl3_lifecycle.hpp>
#include <arisa/provider/sdl3/sdl3_offscreen_surface.hpp>
#include <arisa/provider/sdl3/sdl3_surface_mode.hpp>
#include <arisa/provider/sdl3/sdl3_system_info.hpp>
#include <arisa/provider/sdl3/sdl3_text_input.hpp>
#include <arisa/provider/sdl3/sdl3_window_surface.hpp>
#include <arisa/provider/standard/directory_assets.hpp>
#include <arisa/provider/standard/standard_clock.hpp>
#include <arisa/provider/standard/std_threading.hpp>
#include <arisa/provider/standard/stderr_diagnostics.hpp>
#include <arisa/provider/surface/absent.hpp>
#include <arisa/provider/system_info/absent.hpp>
#include <arisa/provider/text_input/absent.hpp>

#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>

namespace arisa::provider::sdl3 {

class Sdl3ProviderFactory final : public ProviderFactory {
 public:
  static constexpr std::string_view assets_folder = "assets";

  [[nodiscard]] static auto create(Providers overrides = {},
                                   SurfaceMode surface = NoSurface{},
                                   const SoundBank& sounds = {},
                                   const FontBank& fonts = {})
      -> std::expected<Sdl3ProviderFactory, ProviderError>;

  static auto default_assets_root() -> std::filesystem::path;

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

  auto stock_gpu() noexcept -> std::optional<std::reference_wrapper<Sdl3Gpu>>;

 private:
  explicit Sdl3ProviderFactory(Providers overrides);

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

  void create_surface(const SurfaceMode& mode);
  void create_gpu();

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
  std::unique_ptr<standard::StdThreadingProvider> threading_;
  standard::StandardClock clock_;
  std::unique_ptr<standard::StderrDiagnostics> diagnostics_;
  standard::DirectoryAssets assets_;
  std::unique_ptr<Sdl3EventPump> pump_;
  std::unique_ptr<Sdl3WindowSurface> window_surface_;
  std::unique_ptr<Sdl3OffscreenSurface> offscreen_surface_;
  std::optional<Sdl3Lifecycle> lifecycle_;
  std::optional<Sdl3SystemInfo> system_info_;
  std::optional<Sdl3Haptics> haptics_;
  std::optional<Sdl3Audio> audio_;
  std::optional<Sdl3Clipboard> clipboard_;
  std::optional<Sdl3Browser> browser_;
  std::optional<Sdl3Fonts> fonts_;
  std::optional<Sdl3Input> input_;
  std::optional<Sdl3Gpu> gpu_;
  std::optional<Sdl3TextInput> text_input_;
  AbsentLifecycleProvider absent_lifecycle_;
  AbsentSystemInfoProvider absent_system_info_;
  AbsentHapticsProvider absent_haptics_;
  AbsentAudioProvider absent_audio_;
  AbsentClipboardProvider absent_clipboard_;
  AbsentBrowserProvider absent_browser_;
  AbsentFontProvider absent_fonts_;
  AbsentSurfaceProvider absent_surface_;
  AbsentInputProvider absent_input_;
  AbsentGpuProvider absent_gpu_;
  AbsentTextInputProvider absent_text_input_;
};

}
