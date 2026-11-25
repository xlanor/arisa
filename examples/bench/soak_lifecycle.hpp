#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/lifecycle/home_button_block.hpp>
#include <arisa/provider/lifecycle/provider.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string_view>

namespace arisa::examples {

class SoakLifecycle final : public provider::LifecycleProvider {
 public:
  using Observer = std::move_only_function<void(provider::LifecycleEvent)>;

  SoakLifecycle(provider::LifecycleProvider& inner, Observer observer) noexcept
      : inner_(inner), observer_(std::move(observer)) {}

  auto phase() const noexcept -> provider::ScenePhase override {
    return inner_.get().phase();
  }

  auto poll() -> std::optional<provider::LifecycleEvent> override;

  [[nodiscard]] auto block_home_button()
      -> std::expected<provider::HomeButtonBlock,
                       provider::ProviderError> override {
    return inner_.get().block_home_button();
  }

  auto suspensions() const noexcept -> std::uint32_t { return suspensions_; }

  auto resumptions() const noexcept -> std::uint32_t { return resumptions_; }

  auto focus_changes() const noexcept -> std::uint32_t {
    return focus_changes_;
  }

  static auto name(provider::LifecycleEvent event) noexcept -> std::string_view;

 private:
  std::reference_wrapper<provider::LifecycleProvider> inner_;
  Observer observer_;
  std::uint32_t suspensions_{0};
  std::uint32_t resumptions_{0};
  std::uint32_t focus_changes_{0};
};

}
