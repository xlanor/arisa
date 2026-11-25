#pragma once

#include <arisa/provider/factory.hpp>

#include <functional>
#include <optional>

namespace arisa::provider {

struct Providers {
  std::optional<std::reference_wrapper<ThreadingProvider>> threading{};
  std::optional<std::reference_wrapper<ClockProvider>> clock{};
  std::optional<std::reference_wrapper<DiagnosticsProvider>> diagnostics{};
  std::optional<std::reference_wrapper<AssetProvider>> assets{};
  std::optional<std::reference_wrapper<LifecycleProvider>> lifecycle{};
  std::optional<std::reference_wrapper<SystemInfoProvider>> system_info{};
  std::optional<std::reference_wrapper<HapticsProvider>> haptics{};
  std::optional<std::reference_wrapper<AudioProvider>> audio{};
  std::optional<std::reference_wrapper<ClipboardProvider>> clipboard{};
  std::optional<std::reference_wrapper<BrowserProvider>> browser{};
  std::optional<std::reference_wrapper<FontProvider>> fonts{};
  std::optional<std::reference_wrapper<SurfaceProvider>> surface{};
  std::optional<std::reference_wrapper<InputProvider>> input{};
  std::optional<std::reference_wrapper<GpuProvider>> gpu{};
  std::optional<std::reference_wrapper<TextInputProvider>> text_input{};
  std::optional<std::reference_wrapper<AppletProvider>> applets{};
};

}
