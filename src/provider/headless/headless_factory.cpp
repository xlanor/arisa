#include <arisa/provider/headless/headless_factory.hpp>

#include <utility>

namespace arisa::provider::headless {

HeadlessProviderFactory::HeadlessProviderFactory() {
  if (auto probe = AssetPath::create(probe_path)) {
    assets_.add(std::move(*probe), probe_text);
  }
  fonts_.add(SystemFont::standard, standard_face);
}

auto HeadlessProviderFactory::info() const -> ProviderInfo {
  return ProviderInfo{.name = "arisa headless",
                      .abi_version = abi_version,
                      .capabilities = Capability::all};
}

auto HeadlessProviderFactory::threading() noexcept
    -> standard::StdThreadingProvider& {
  return threading_;
}

auto HeadlessProviderFactory::clock() noexcept -> ManualClock& {
  return clock_;
}

auto HeadlessProviderFactory::diagnostics() noexcept -> RecordingDiagnostics& {
  return diagnostics_;
}

auto HeadlessProviderFactory::assets() noexcept -> MemoryAssets& {
  return assets_;
}

auto HeadlessProviderFactory::lifecycle() noexcept -> ScriptedLifecycle& {
  return lifecycle_;
}

auto HeadlessProviderFactory::system_info() noexcept -> ScriptedSystemInfo& {
  return system_info_;
}

auto HeadlessProviderFactory::haptics() noexcept -> RecordingHaptics& {
  return haptics_;
}

auto HeadlessProviderFactory::audio() noexcept -> RecordingAudio& {
  return audio_;
}

auto HeadlessProviderFactory::clipboard() noexcept -> MemoryClipboard& {
  return clipboard_;
}

auto HeadlessProviderFactory::browser() noexcept -> RecordingBrowser& {
  return browser_;
}

auto HeadlessProviderFactory::applets() noexcept -> ScriptedApplets& {
  return applets_;
}

auto HeadlessProviderFactory::speech() noexcept -> RecordingSpeech& {
  return speech_;
}

auto HeadlessProviderFactory::fonts() noexcept -> MemoryFonts& {
  return fonts_;
}

auto HeadlessProviderFactory::surface() noexcept -> ScriptedSurface& {
  return surface_;
}

auto HeadlessProviderFactory::input() noexcept -> ScriptedInput& {
  return input_;
}

auto HeadlessProviderFactory::gpu() noexcept -> NullGpu& { return gpu_; }

auto HeadlessProviderFactory::text_input() noexcept -> ScriptedTextInput& {
  return text_input_;
}

}
