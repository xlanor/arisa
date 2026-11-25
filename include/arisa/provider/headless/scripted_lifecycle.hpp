#pragma once

#include <arisa/provider/lifecycle/provider.hpp>

#include <atomic>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>

namespace arisa::provider::headless {

class ScriptedLifecycle final : public LifecycleProvider {
 public:
  auto phase() const noexcept -> ScenePhase override;

  auto poll() -> std::optional<LifecycleEvent> override;

  [[nodiscard]] auto block_home_button()
      -> std::expected<HomeButtonBlock, ProviderError> override;

  void push(LifecycleEvent event);

  void set_phase(ScenePhase phase) noexcept;

  auto live_home_button_blocks() const noexcept -> std::size_t;

 private:
  std::atomic<ScenePhase> phase_{ScenePhase::active};
  std::mutex mutex_;
  std::deque<LifecycleEvent> pending_;
  std::shared_ptr<std::atomic<std::size_t>> live_blocks_{
      std::make_shared<std::atomic<std::size_t>>(0)};
};

}
