#include <arisa/provider/sdl3/sdl3_factory.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>

#include <format>
#include <functional>
#include <system_error>
#include <utility>
#include <variant>

namespace arisa::provider::sdl3 {

Sdl3ProviderFactory::Sdl3ProviderFactory(Providers overrides)
    : overrides_(overrides),
      threading_(std::make_unique<standard::StdThreadingProvider>()),
      diagnostics_(std::make_unique<standard::StderrDiagnostics>()),
      assets_(default_assets_root()) {}

auto Sdl3ProviderFactory::create(Providers overrides, SurfaceMode surface,
                                 const SoundBank& sounds, const FontBank& fonts)
    -> std::expected<Sdl3ProviderFactory, ProviderError> {
  Sdl3ProviderFactory factory{overrides};
  auto pump = Sdl3EventPump::create(factory.threading());
  if (pump.has_value()) {
    factory.pump_ = std::make_unique<Sdl3EventPump>(std::move(*pump));
  } else {
    factory.report_unavailable("events", pump.error());
  }
  if (!overrides.lifecycle.has_value() && factory.pump_ != nullptr) {
    factory.lifecycle_.emplace(*factory.pump_);
  }
  if (!overrides.system_info.has_value()) {
    factory.adopt(factory.system_info_, Sdl3SystemInfo::create(),
                  "system_info");
  }
  if (!overrides.haptics.has_value()) {
    factory.adopt(factory.haptics_, Sdl3Haptics::create(), "haptics");
  }
  if (!overrides.audio.has_value()) {
    factory.adopt(factory.audio_,
                  Sdl3Audio::create(factory.assets(), factory.diagnostics(),
                                    sounds, std::ref(factory.threading())),
                  "audio");
  }
  if (!overrides.clipboard.has_value()) {
    factory.adopt(factory.clipboard_, Sdl3Clipboard::create(), "clipboard");
  }
  if (!overrides.browser.has_value()) {
    factory.adopt(factory.browser_, Sdl3Browser::create(), "browser");
  }
  if (!overrides.fonts.has_value()) {
    factory.adopt(factory.fonts_, Sdl3Fonts::create(factory.assets(), fonts),
                  "fonts");
  }
  if (!overrides.surface.has_value()) {
    factory.create_surface(surface);
  }
  if (!overrides.input.has_value() && factory.pump_ != nullptr) {
    factory.adopt(factory.input_, Sdl3Input::create(*factory.pump_), "input");
  }
  if (!overrides.gpu.has_value()) {
    factory.create_gpu();
  }
  if (!overrides.text_input.has_value() && factory.pump_ != nullptr) {
    factory.text_input_.emplace(*factory.pump_,
                                factory.window_surface_ != nullptr
                                    ? factory.window_surface_->window()
                                    : nullptr);
  }
  return factory;
}

void Sdl3ProviderFactory::create_surface(const SurfaceMode& mode) {
  if (pump_ == nullptr) {
    return;
  }
  if (const auto* window = std::get_if<WindowMode>(&mode)) {
    auto created =
        Sdl3WindowSurface::create(*pump_, window->title, window->size);
    if (created.has_value()) {
      window_surface_ =
          std::make_unique<Sdl3WindowSurface>(std::move(*created));
    } else {
      report_unavailable("surface", created.error());
    }
  } else if (const auto* offscreen = std::get_if<OffscreenMode>(&mode)) {
    auto created = Sdl3OffscreenSurface::create(*pump_, offscreen->size);
    if (created.has_value()) {
      offscreen_surface_ =
          std::make_unique<Sdl3OffscreenSurface>(std::move(*created));
    } else {
      report_unavailable("surface", created.error());
    }
  }
}

void Sdl3ProviderFactory::create_gpu() {
  if (window_surface_ != nullptr) {
    adopt(gpu_, Sdl3Gpu::create(*window_surface_), "gpu");
  } else if (offscreen_surface_ != nullptr) {
    adopt(gpu_, Sdl3Gpu::create(*offscreen_surface_), "gpu");
  }
}

auto Sdl3ProviderFactory::default_assets_root() -> std::filesystem::path {
  if (const char* base = SDL_GetBasePath(); base != nullptr) {
    return std::filesystem::path{base} / assets_folder;
  }
  std::error_code error;
  const std::filesystem::path working = std::filesystem::current_path(error);
  return (error ? std::filesystem::path{"."} : working) / assets_folder;
}

auto Sdl3ProviderFactory::info() const -> ProviderInfo {
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
  supply_if(supplied(overrides_.clipboard, clipboard_.has_value()),
            Capability::clipboard);
  supply_if(supplied(overrides_.browser, browser_.has_value()),
            Capability::browser);
  supply_if(supplied(overrides_.fonts, fonts_.has_value()), Capability::fonts);
  supply_if(supplied(overrides_.surface, window_surface_ != nullptr ||
                                             offscreen_surface_ != nullptr),
            Capability::surface);
  supply_if(supplied(overrides_.input, input_.has_value()), Capability::input);
  supply_if(supplied(overrides_.gpu, gpu_.has_value()), Capability::gpu);
  supply_if(supplied(overrides_.text_input, text_input_.has_value()),
            Capability::text_input);
  return ProviderInfo{.name = "arisa sdl3",
                      .abi_version = abi_version,
                      .capabilities = capabilities};
}

auto Sdl3ProviderFactory::threading() noexcept -> ThreadingProvider& {
  return overrides_.threading.has_value() ? overrides_.threading->get()
                                          : *threading_;
}

auto Sdl3ProviderFactory::clock() noexcept -> ClockProvider& {
  return overrides_.clock.has_value() ? overrides_.clock->get() : clock_;
}

auto Sdl3ProviderFactory::diagnostics() noexcept -> DiagnosticsProvider& {
  return overrides_.diagnostics.has_value() ? overrides_.diagnostics->get()
                                            : *diagnostics_;
}

auto Sdl3ProviderFactory::assets() noexcept -> AssetProvider& {
  return overrides_.assets.has_value() ? overrides_.assets->get() : assets_;
}

auto Sdl3ProviderFactory::lifecycle() noexcept -> LifecycleProvider& {
  return select(overrides_.lifecycle, lifecycle_, absent_lifecycle_);
}

auto Sdl3ProviderFactory::system_info() noexcept -> SystemInfoProvider& {
  return select(overrides_.system_info, system_info_, absent_system_info_);
}

auto Sdl3ProviderFactory::haptics() noexcept -> HapticsProvider& {
  return select(overrides_.haptics, haptics_, absent_haptics_);
}

auto Sdl3ProviderFactory::audio() noexcept -> AudioProvider& {
  return select(overrides_.audio, audio_, absent_audio_);
}

auto Sdl3ProviderFactory::clipboard() noexcept -> ClipboardProvider& {
  return select(overrides_.clipboard, clipboard_, absent_clipboard_);
}

auto Sdl3ProviderFactory::browser() noexcept -> BrowserProvider& {
  return select(overrides_.browser, browser_, absent_browser_);
}

auto Sdl3ProviderFactory::fonts() noexcept -> FontProvider& {
  return select(overrides_.fonts, fonts_, absent_fonts_);
}

auto Sdl3ProviderFactory::surface() noexcept -> SurfaceProvider& {
  if (overrides_.surface.has_value()) {
    return overrides_.surface->get();
  }
  if (window_surface_ != nullptr) {
    return *window_surface_;
  }
  if (offscreen_surface_ != nullptr) {
    return *offscreen_surface_;
  }
  return absent_surface_;
}

auto Sdl3ProviderFactory::input() noexcept -> InputProvider& {
  return select(overrides_.input, input_, absent_input_);
}

auto Sdl3ProviderFactory::gpu() noexcept -> GpuProvider& {
  return select(overrides_.gpu, gpu_, absent_gpu_);
}

auto Sdl3ProviderFactory::text_input() noexcept -> TextInputProvider& {
  return select(overrides_.text_input, text_input_, absent_text_input_);
}

auto Sdl3ProviderFactory::stock_gpu() noexcept
    -> std::optional<std::reference_wrapper<Sdl3Gpu>> {
  if (overrides_.gpu.has_value() || !gpu_.has_value()) {
    return std::nullopt;
  }
  return std::ref(*gpu_);
}

void Sdl3ProviderFactory::report_unavailable(std::string_view capability,
                                             const ProviderError& error) {
  const std::string_view platform =
      error.code() == ProviderErrorCode::platform_failure ? SDL_GetError() : "";
  diagnostics().report(Diagnostic{
      .severity = Severity::warning,
      .category = "arisa.sdl3",
      .message = platform.empty()
                     ? std::format("{} unavailable: {}", capability,
                                   error.description())
                     : std::format("{} unavailable: {} ({})", capability,
                                   error.description(), platform)});
}

}
