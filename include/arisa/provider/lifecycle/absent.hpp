#pragma once

#include <arisa/provider/lifecycle/provider.hpp>

namespace arisa::provider {

class AbsentLifecycleProvider final : public LifecycleProvider {
 public:
  auto phase() const noexcept -> ScenePhase override;
  auto poll() -> std::optional<LifecycleEvent> override;
  [[nodiscard]] auto block_home_button()
      -> std::expected<HomeButtonBlock, ProviderError> override;
};

}
