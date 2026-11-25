#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/lifecycle/home_button_block.hpp>

#include <cstdint>
#include <expected>
#include <optional>

namespace arisa::provider {

enum class ScenePhase : std::uint8_t {
  active,
  inactive,
  background,
};

enum class LifecycleEvent : std::uint8_t {
  focus_gained,
  focus_lost,
  suspending,
  resumed,
  exit_requested,
  operation_mode_changed,
};

class LifecycleProvider {
 public:
  virtual ~LifecycleProvider() = default;

  virtual auto phase() const noexcept -> ScenePhase = 0;

  virtual auto poll() -> std::optional<LifecycleEvent> = 0;

  [[nodiscard]] virtual auto block_home_button()
      -> std::expected<HomeButtonBlock, ProviderError> = 0;
};

}
