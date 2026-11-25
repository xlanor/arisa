#include <arisa/provider/default_factory.hpp>

namespace arisa::provider {

DefaultProviderFactory::DefaultProviderFactory(Providers overrides)
    : overrides_(overrides),
      assets_(standard::DirectoryAssets::current_directory()) {}

auto DefaultProviderFactory::info() const -> ProviderInfo {
  Capability supplied = Capability::required;
  const auto supply_if = [&supplied](bool present, Capability capability) {
    if (present) {
      supplied |= capability;
    }
  };
  supply_if(overrides_.lifecycle.has_value(), Capability::lifecycle);
  supply_if(overrides_.system_info.has_value(), Capability::system_info);
  supply_if(overrides_.haptics.has_value(), Capability::haptics);
  supply_if(overrides_.audio.has_value(), Capability::audio);
  supply_if(overrides_.clipboard.has_value(), Capability::clipboard);
  supply_if(overrides_.browser.has_value(), Capability::browser);
  supply_if(overrides_.fonts.has_value(), Capability::fonts);
  supply_if(overrides_.surface.has_value(), Capability::surface);
  supply_if(overrides_.input.has_value(), Capability::input);
  supply_if(overrides_.gpu.has_value(), Capability::gpu);
  supply_if(overrides_.text_input.has_value(), Capability::text_input);
  return ProviderInfo{.name = "arisa default",
                      .abi_version = abi_version,
                      .capabilities = supplied};
}

auto DefaultProviderFactory::threading() noexcept -> ThreadingProvider& {
  return select(overrides_.threading, threading_);
}

auto DefaultProviderFactory::clock() noexcept -> ClockProvider& {
  return select(overrides_.clock, clock_);
}

auto DefaultProviderFactory::diagnostics() noexcept -> DiagnosticsProvider& {
  return select(overrides_.diagnostics, diagnostics_);
}

auto DefaultProviderFactory::assets() noexcept -> AssetProvider& {
  return select(overrides_.assets, assets_);
}

auto DefaultProviderFactory::lifecycle() noexcept -> LifecycleProvider& {
  return select(overrides_.lifecycle, lifecycle_);
}

auto DefaultProviderFactory::system_info() noexcept -> SystemInfoProvider& {
  return select(overrides_.system_info, system_info_);
}

auto DefaultProviderFactory::haptics() noexcept -> HapticsProvider& {
  return select(overrides_.haptics, haptics_);
}

auto DefaultProviderFactory::audio() noexcept -> AudioProvider& {
  return select(overrides_.audio, audio_);
}

auto DefaultProviderFactory::clipboard() noexcept -> ClipboardProvider& {
  return select(overrides_.clipboard, clipboard_);
}

auto DefaultProviderFactory::browser() noexcept -> BrowserProvider& {
  return select(overrides_.browser, browser_);
}

auto DefaultProviderFactory::fonts() noexcept -> FontProvider& {
  return select(overrides_.fonts, fonts_);
}

auto DefaultProviderFactory::surface() noexcept -> SurfaceProvider& {
  return select(overrides_.surface, surface_);
}

auto DefaultProviderFactory::input() noexcept -> InputProvider& {
  return select(overrides_.input, input_);
}

auto DefaultProviderFactory::gpu() noexcept -> GpuProvider& {
  return select(overrides_.gpu, gpu_);
}

auto DefaultProviderFactory::text_input() noexcept -> TextInputProvider& {
  return select(overrides_.text_input, text_input_);
}

}
