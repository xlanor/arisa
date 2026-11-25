#include <real_time_providers.hpp>

namespace arisa::examples {

auto RealTimeProviders::info() const -> provider::ProviderInfo {
  return headless_.get().info();
}

auto RealTimeProviders::threading() noexcept -> provider::ThreadingProvider& {
  return headless_.get().threading();
}

auto RealTimeProviders::clock() noexcept -> provider::ClockProvider& {
  return clock_;
}

auto RealTimeProviders::diagnostics() noexcept
    -> provider::DiagnosticsProvider& {
  return headless_.get().diagnostics();
}

auto RealTimeProviders::assets() noexcept -> provider::AssetProvider& {
  return headless_.get().assets();
}

auto RealTimeProviders::lifecycle() noexcept -> provider::LifecycleProvider& {
  return headless_.get().lifecycle();
}

auto RealTimeProviders::system_info() noexcept
    -> provider::SystemInfoProvider& {
  return headless_.get().system_info();
}

auto RealTimeProviders::haptics() noexcept -> provider::HapticsProvider& {
  return headless_.get().haptics();
}

auto RealTimeProviders::audio() noexcept -> provider::AudioProvider& {
  return headless_.get().audio();
}

auto RealTimeProviders::clipboard() noexcept -> provider::ClipboardProvider& {
  return headless_.get().clipboard();
}

auto RealTimeProviders::browser() noexcept -> provider::BrowserProvider& {
  return headless_.get().browser();
}

auto RealTimeProviders::fonts() noexcept -> provider::FontProvider& {
  return headless_.get().fonts();
}

auto RealTimeProviders::surface() noexcept -> provider::SurfaceProvider& {
  return headless_.get().surface();
}

auto RealTimeProviders::input() noexcept -> provider::InputProvider& {
  return headless_.get().input();
}

auto RealTimeProviders::gpu() noexcept -> provider::GpuProvider& {
  return headless_.get().gpu();
}

auto RealTimeProviders::text_input() noexcept -> provider::TextInputProvider& {
  return headless_.get().text_input();
}

}
