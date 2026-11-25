#include <arisa/provider/lifecycle/absent.hpp>

namespace arisa::provider {

auto AbsentLifecycleProvider::phase() const noexcept -> ScenePhase {
  return ScenePhase::active;
}

auto AbsentLifecycleProvider::poll() -> std::optional<LifecycleEvent> {
  return std::nullopt;
}

auto AbsentLifecycleProvider::block_home_button()
    -> std::expected<HomeButtonBlock, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

}
