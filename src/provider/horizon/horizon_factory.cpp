#include <arisa/provider/horizon/horizon_factory.hpp>

#include <filesystem>
#include <format>
#include <string>
#include <system_error>
#include <utility>

namespace arisa::provider::horizon {

HorizonProviderFactory::HorizonProviderFactory(
    Providers overrides, std::optional<ServiceSession<RomfsService>> romfs,
    std::optional<HorizonClock> clock)
    : overrides_(overrides),
      romfs_(std::move(romfs)),
      threading_(Console::placement()),
      clock_(std::move(clock)),
      diagnostics_(std::make_unique<standard::StderrDiagnostics>()),
      assets_(std::filesystem::path{romfs_root}),
      absent_surface_(std::make_unique<AbsentSurfaceProvider>()) {}

auto HorizonProviderFactory::create(Providers overrides,
                                    const SoundBank& sounds, GpuOptions gpu)
    -> std::expected<HorizonProviderFactory, ProviderError> {
  std::optional<ServiceSession<RomfsService>> romfs;
  if (!overrides.assets.has_value()) {
    auto mounted = mount_romfs();
    if (!mounted.has_value()) {
      return std::unexpected(mounted.error());
    }
    romfs = std::move(*mounted);
  }
  std::optional<HorizonClock> clock;
  if (!overrides.clock.has_value()) {
    auto created = HorizonClock::create();
    if (!created.has_value()) {
      return std::unexpected(created.error());
    }
    clock.emplace(std::move(*created));
  }
  HorizonProviderFactory factory{overrides, std::move(romfs), std::move(clock)};
  if (!overrides.lifecycle.has_value()) {
    factory.lifecycle_.emplace();
  }
  if (!overrides.system_info.has_value()) {
    factory.adopt(factory.system_info_, HorizonSystemInfo::create(),
                  "system_info");
  }
  if (!overrides.haptics.has_value()) {
    factory.adopt(factory.haptics_, HorizonHaptics::create(factory.threading()),
                  "haptics");
  }
  if (!overrides.audio.has_value()) {
    factory.adopt(factory.audio_,
                  HorizonAudio::create(factory.assets(), factory.diagnostics(),
                                       sounds, std::ref(factory.threading())),
                  "audio");
  }
  if (!overrides.applets.has_value()) {
    factory.adopt(factory.applets_, HorizonApplets::create(), "applets");
  }
  if (!overrides.browser.has_value()) {
    factory.adopt(factory.browser_, HorizonBrowser::create(), "browser");
  }
  if (!overrides.fonts.has_value()) {
    factory.adopt(factory.fonts_, HorizonFonts::create(), "fonts");
  }
  if (!overrides.surface.has_value()) {
    auto surface = HorizonSurface::create(gpu.docked_cap);
    if (surface.has_value()) {
      factory.surface_ = std::make_unique<HorizonSurface>(std::move(*surface));
#if defined(ARISA_HORIZON_DEKO3D) && defined(__SWITCH2__) && \
    !defined(ARISA_HORIZON_VULKAN)
      factory.surface_->release_layer_on_close();
#endif
    } else {
      factory.report_unavailable("surface", surface.error());
    }
  }
  if (!overrides.input.has_value()) {
    factory.input_.emplace(factory.surface(), InputTuning{},
                           factory.keyboard_shown_);
    if (const auto failed = factory.input_->pointer_failure()) {
      factory.report_unavailable("pointer", *failed);
    }
  }
  if (!overrides.gpu.has_value() && factory.surface_ != nullptr) {
    factory.open_gpu(gpu);
  }
  if (!overrides.text_input.has_value()) {
    factory.adopt(
        factory.text_input_,
        HorizonTextInput::create(factory.threading(), factory.keyboard_shown_),
        "text_input");
  }
  return factory;
}

auto HorizonProviderFactory::info() const -> ProviderInfo {
  Capability capabilities = Capability::required;
  const auto supply_if = [&capabilities](bool present, Capability capability) {
    if (present) {
      capabilities |= capability;
    }
  };
  supply_if(supplied(overrides_.lifecycle, lifecycle_.has_value()),
            Capability::lifecycle);
  supply_if(supplied(overrides_.system_info, system_info_.has_value()),
            Capability::system_info);
  supply_if(supplied(overrides_.haptics, haptics_.has_value()),
            Capability::haptics);
  supply_if(supplied(overrides_.audio, audio_.has_value()), Capability::audio);
  supply_if(supplied(overrides_.clipboard, false), Capability::clipboard);
  supply_if(supplied(overrides_.browser, browser_.has_value()),
            Capability::browser);
  supply_if(supplied(overrides_.applets, applets_.has_value()),
            Capability::applets);
  supply_if(supplied(overrides_.fonts, fonts_.has_value()), Capability::fonts);
  supply_if(supplied(overrides_.surface, surface_ != nullptr),
            Capability::surface);
  supply_if(supplied(overrides_.input, input_.has_value()), Capability::input);
  supply_if(supplied(overrides_.gpu, gpu_api_.has_value()), Capability::gpu);
  supply_if(supplied(overrides_.text_input, text_input_.has_value()),
            Capability::text_input);
  std::string_view name = Console::provider_name;
#if defined(ARISA_HORIZON_DEKO3D) && defined(__SWITCH2__)
  if (gpu_api_ == GpuApi::drako3d) {
    name = ounce::DrakoConsole::provider_name;
  }
#endif
  return ProviderInfo{.name = std::string{name},
                      .abi_version = abi_version,
                      .capabilities = capabilities};
}

auto HorizonProviderFactory::threading() noexcept -> ThreadingProvider& {
  return overrides_.threading.has_value() ? overrides_.threading->get()
                                          : threading_;
}

auto HorizonProviderFactory::clock() noexcept -> ClockProvider& {
  return overrides_.clock.has_value() ? overrides_.clock->get() : *clock_;
}

auto HorizonProviderFactory::diagnostics() noexcept -> DiagnosticsProvider& {
  return overrides_.diagnostics.has_value() ? overrides_.diagnostics->get()
                                            : *diagnostics_;
}

auto HorizonProviderFactory::assets() noexcept -> AssetProvider& {
  return overrides_.assets.has_value() ? overrides_.assets->get() : assets_;
}

auto HorizonProviderFactory::lifecycle() noexcept -> LifecycleProvider& {
  return select(overrides_.lifecycle, lifecycle_, absent_lifecycle_);
}

auto HorizonProviderFactory::system_info() noexcept -> SystemInfoProvider& {
  return select(overrides_.system_info, system_info_, absent_system_info_);
}

auto HorizonProviderFactory::haptics() noexcept -> HapticsProvider& {
  return select(overrides_.haptics, haptics_, absent_haptics_);
}

auto HorizonProviderFactory::audio() noexcept -> AudioProvider& {
  return select(overrides_.audio, audio_, absent_audio_);
}

auto HorizonProviderFactory::clipboard() noexcept -> ClipboardProvider& {
  return overrides_.clipboard.has_value() ? overrides_.clipboard->get()
                                          : absent_clipboard_;
}

auto HorizonProviderFactory::browser() noexcept -> BrowserProvider& {
  return select(overrides_.browser, browser_, absent_browser_);
}

auto HorizonProviderFactory::applets() noexcept -> AppletProvider& {
  return select(overrides_.applets, applets_, absent_applets_);
}

auto HorizonProviderFactory::fonts() noexcept -> FontProvider& {
  return select(overrides_.fonts, fonts_, absent_fonts_);
}

auto HorizonProviderFactory::surface() noexcept -> SurfaceProvider& {
  if (overrides_.surface.has_value()) {
    return overrides_.surface->get();
  }
  if (surface_ != nullptr) {
    return *surface_;
  }
  return *absent_surface_;
}

auto HorizonProviderFactory::input() noexcept -> InputProvider& {
  return select(overrides_.input, input_, absent_input_);
}

auto HorizonProviderFactory::gpu() noexcept -> GpuProvider& {
  if (overrides_.gpu.has_value()) {
    return overrides_.gpu->get();
  }
#if defined(ARISA_HORIZON_DEKO3D)
  if (deko3d_.has_value()) {
    return *deko3d_;
  }
#endif
#if defined(ARISA_HORIZON_VULKAN)
  if (vulkan_.has_value()) {
    return *vulkan_;
  }
#endif
  return absent_gpu_;
}

void HorizonProviderFactory::open_gpu(const GpuOptions& options) {
  const GpuApi api = options.api.value_or(default_gpu_api());
  if (!supports(api)) {
    report_unavailable("gpu", ProviderError::unsupported_capability());
    return;
  }
  switch (api) {
    case GpuApi::deko3d:
    case GpuApi::drako3d:
#if defined(ARISA_HORIZON_DEKO3D) && defined(__SWITCH2__)
      surface_->release_layer_on_close();
      adopt(deko3d_,
            ounce::DrakoConsole::create_gpu(*surface_, options, diagnostics()),
            "gpu");
#elif defined(ARISA_HORIZON_DEKO3D)
      adopt(deko3d_, Console::create_gpu(*surface_, options, diagnostics()),
            "gpu");
#endif
#if defined(ARISA_HORIZON_DEKO3D)
      if (deko3d_.has_value()) {
        gpu_api_ = api;
      }
#endif
      return;
    case GpuApi::vulkan:
#if defined(ARISA_HORIZON_VULKAN)
      adopt(vulkan_,
            ounce::OunceConsole::create_gpu(*surface_, options, diagnostics()),
            "gpu");
      if (vulkan_.has_value()) {
        gpu_api_ = api;
      }
#endif
      return;
  }
}

auto HorizonProviderFactory::text_input() noexcept -> TextInputProvider& {
  return select(overrides_.text_input, text_input_, absent_text_input_);
}

auto HorizonProviderFactory::stock_gpu_report() -> std::optional<std::string> {
#if defined(ARISA_HORIZON_VULKAN)
  if (const auto chosen = stock_gpu<vulkan::VulkanGpu>()) {
    return ounce::OunceConsole::gpu_report(chosen->get());
  }
#endif
#if defined(ARISA_HORIZON_DEKO3D) && defined(__SWITCH2__)
  if (const auto chosen = stock_gpu<Deko3dGpu>()) {
    return ounce::DrakoConsole::gpu_report(chosen->get());
  }
#elif defined(ARISA_HORIZON_DEKO3D)
  if (const auto chosen = stock_gpu<Deko3dGpu>()) {
    return Console::gpu_report(chosen->get());
  }
#endif
  return std::nullopt;
}

auto HorizonProviderFactory::mount_romfs()
    -> std::expected<std::optional<ServiceSession<RomfsService>>,
                     ProviderError> {
  auto mounted = ServiceSession<RomfsService>::create();
  if (mounted.has_value()) {
    return std::optional<ServiceSession<RomfsService>>{std::move(*mounted)};
  }
  std::error_code error;
  if (std::filesystem::is_directory(std::filesystem::path{romfs_root}, error)) {
    return std::optional<ServiceSession<RomfsService>>{};
  }
  return std::unexpected(mounted.error());
}

void HorizonProviderFactory::report_unavailable(std::string_view capability,
                                                const ProviderError& error) {
  diagnostics().report(Diagnostic{
      .severity = Severity::warning,
      .category = "arisa.horizon",
      .message =
          std::format("{} unavailable: {} (result {:#x})", capability,
                      error.description(), error.native_result().value_or(0))});
}

}
