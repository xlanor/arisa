#pragma once

#include <arisa/provider/factory.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/standard/standard_clock.hpp>

#include <functional>

namespace arisa::examples {

class RealTimeProviders final : public provider::ProviderFactory {
 public:
  explicit RealTimeProviders(
      provider::headless::HeadlessProviderFactory& headless) noexcept
      : headless_(headless) {}

  auto info() const -> provider::ProviderInfo override;

  auto threading() noexcept -> provider::ThreadingProvider& override;
  auto clock() noexcept -> provider::ClockProvider& override;
  auto diagnostics() noexcept -> provider::DiagnosticsProvider& override;
  auto assets() noexcept -> provider::AssetProvider& override;
  auto lifecycle() noexcept -> provider::LifecycleProvider& override;
  auto system_info() noexcept -> provider::SystemInfoProvider& override;
  auto haptics() noexcept -> provider::HapticsProvider& override;
  auto audio() noexcept -> provider::AudioProvider& override;
  auto clipboard() noexcept -> provider::ClipboardProvider& override;
  auto browser() noexcept -> provider::BrowserProvider& override;
  auto fonts() noexcept -> provider::FontProvider& override;
  auto surface() noexcept -> provider::SurfaceProvider& override;
  auto input() noexcept -> provider::InputProvider& override;
  auto gpu() noexcept -> provider::GpuProvider& override;
  auto text_input() noexcept -> provider::TextInputProvider& override;

 private:
  std::reference_wrapper<provider::headless::HeadlessProviderFactory> headless_;
  provider::standard::StandardClock clock_;
};

}
