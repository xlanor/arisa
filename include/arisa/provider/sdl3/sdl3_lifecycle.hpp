#pragma once

#include <arisa/provider/lifecycle/provider.hpp>
#include <arisa/provider/sdl3/sdl3_event_pump.hpp>

#include <SDL3/SDL_events.h>

#include <functional>
#include <optional>

namespace arisa::provider::sdl3 {

class Sdl3Lifecycle final : public LifecycleProvider {
 public:
  explicit Sdl3Lifecycle(Sdl3EventPump& pump);

  auto phase() const noexcept -> ScenePhase override;

  auto poll() -> std::optional<LifecycleEvent> override;

  [[nodiscard]] auto block_home_button()
      -> std::expected<HomeButtonBlock, ProviderError> override;

  static auto translate(const SDL_Event& event) noexcept
      -> std::optional<LifecycleEvent>;

 private:
  void track(LifecycleEvent event) noexcept;

  std::reference_wrapper<Sdl3EventPump> pump_;
  Sdl3EventPump::Subscription events_;
  bool focused_{true};
  bool background_{false};
};

}
