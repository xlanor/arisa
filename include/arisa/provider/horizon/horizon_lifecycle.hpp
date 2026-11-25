#pragma once

#include <arisa/provider/counted_scope.hpp>
#include <arisa/provider/lifecycle/provider.hpp>

#include <memory>

namespace arisa::provider::horizon {

class HorizonLifecycle final : public LifecycleProvider {
 public:
  HorizonLifecycle();

  HorizonLifecycle(const HorizonLifecycle&) = delete;
  auto operator=(const HorizonLifecycle&) -> HorizonLifecycle& = delete;
  HorizonLifecycle(HorizonLifecycle&& other) noexcept;
  auto operator=(HorizonLifecycle&& other) noexcept -> HorizonLifecycle&;
  ~HorizonLifecycle() override;

  auto phase() const noexcept -> ScenePhase override;

  auto poll() -> std::optional<LifecycleEvent> override;

  [[nodiscard]] auto block_home_button()
      -> std::expected<HomeButtonBlock, ProviderError> override;

 private:
  class Hook;

  static auto application() noexcept -> bool;

  std::unique_ptr<Hook> hook_;
  CountedScope home_button_;
};

}
