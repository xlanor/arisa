#pragma once

#include <arisa/provider/factory.hpp>
#include <arisa/provider/headless/manual_clock.hpp>
#include <arisa/provider/headless/memory_assets.hpp>
#include <arisa/provider/headless/memory_clipboard.hpp>
#include <arisa/provider/headless/memory_fonts.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/headless/recording_audio.hpp>
#include <arisa/provider/headless/recording_browser.hpp>
#include <arisa/provider/headless/recording_diagnostics.hpp>
#include <arisa/provider/headless/recording_haptics.hpp>
#include <arisa/provider/headless/recording_speech.hpp>
#include <arisa/provider/headless/scripted_applets.hpp>
#include <arisa/provider/headless/scripted_input.hpp>
#include <arisa/provider/headless/scripted_lifecycle.hpp>
#include <arisa/provider/headless/scripted_surface.hpp>
#include <arisa/provider/headless/scripted_system_info.hpp>
#include <arisa/provider/headless/scripted_text_input.hpp>
#include <arisa/provider/standard/std_threading.hpp>

#include <array>
#include <cstddef>
#include <string_view>

namespace arisa::provider::headless {

class HeadlessProviderFactory final : public ProviderFactory {
 public:
  static constexpr std::string_view probe_path = "arisa-conformance/probe.txt";
  static constexpr std::string_view probe_text = "arisa";
  static constexpr std::array standard_face{
      std::byte{0x00}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};

  HeadlessProviderFactory();

  auto info() const -> ProviderInfo override;

  auto threading() noexcept -> standard::StdThreadingProvider& override;
  auto clock() noexcept -> ManualClock& override;
  auto diagnostics() noexcept -> RecordingDiagnostics& override;
  auto assets() noexcept -> MemoryAssets& override;
  auto lifecycle() noexcept -> ScriptedLifecycle& override;
  auto system_info() noexcept -> ScriptedSystemInfo& override;
  auto haptics() noexcept -> RecordingHaptics& override;
  auto audio() noexcept -> RecordingAudio& override;
  auto clipboard() noexcept -> MemoryClipboard& override;
  auto browser() noexcept -> RecordingBrowser& override;
  auto fonts() noexcept -> MemoryFonts& override;
  auto surface() noexcept -> ScriptedSurface& override;
  auto input() noexcept -> ScriptedInput& override;
  auto gpu() noexcept -> NullGpu& override;
  auto text_input() noexcept -> ScriptedTextInput& override;
  auto applets() noexcept -> ScriptedApplets& override;
  auto speech() noexcept -> RecordingSpeech& override;

 private:
  standard::StdThreadingProvider threading_;
  ManualClock clock_;
  RecordingDiagnostics diagnostics_;
  MemoryAssets assets_;
  ScriptedLifecycle lifecycle_;
  ScriptedSystemInfo system_info_;
  RecordingHaptics haptics_;
  RecordingAudio audio_;
  MemoryClipboard clipboard_;
  RecordingBrowser browser_;
  ScriptedApplets applets_;
  RecordingSpeech speech_;
  MemoryFonts fonts_;
  ScriptedSurface surface_;
  ScriptedInput input_;
  NullGpu gpu_;
  ScriptedTextInput text_input_;
};

}
