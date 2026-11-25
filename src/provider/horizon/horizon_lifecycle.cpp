#include <arisa/provider/horizon/horizon_lifecycle.hpp>

#include <switch.h>

#include <deque>
#include <mutex>
#include <utility>

namespace arisa::provider::horizon {

class HorizonLifecycle::Hook {
 public:
  Hook() noexcept { appletHook(&cookie_, &Hook::on_hook, this); }

  Hook(const Hook&) = delete;
  auto operator=(const Hook&) -> Hook& = delete;
  Hook(Hook&&) = delete;
  auto operator=(Hook&&) -> Hook& = delete;

  ~Hook() { appletUnhook(&cookie_); }

  auto poll() -> std::optional<LifecycleEvent> {
    const std::scoped_lock lock{mutex_};
    if (pending_.empty()) {
      return std::nullopt;
    }
    const LifecycleEvent event = pending_.front();
    pending_.pop_front();
    return event;
  }

 private:
  static void on_hook(AppletHookType type, void* param) {
    auto& self = *static_cast<Hook*>(param);
    if (const std::optional<LifecycleEvent> event = translate(type)) {
      const std::scoped_lock lock{self.mutex_};
      self.pending_.push_back(*event);
    }
  }

  static auto translate(AppletHookType type) noexcept
      -> std::optional<LifecycleEvent> {
    switch (type) {
      case AppletHookType_OnFocusState:
        switch (appletGetFocusState()) {
          case AppletFocusState_InFocus:
            return LifecycleEvent::focus_gained;
          case AppletFocusState_OutOfFocus:
            return LifecycleEvent::focus_lost;
          case AppletFocusState_Background:
            return LifecycleEvent::suspending;
        }
        return std::nullopt;
      case AppletHookType_OnOperationMode:
        return LifecycleEvent::operation_mode_changed;
      case AppletHookType_OnExitRequest:
        return LifecycleEvent::exit_requested;
      case AppletHookType_OnResume:
        return LifecycleEvent::resumed;
      default:
        return std::nullopt;
    }
  }

  AppletHookCookie cookie_{};
  std::mutex mutex_;
  std::deque<LifecycleEvent> pending_;
};

HorizonLifecycle::HorizonLifecycle()
    : hook_(std::make_unique<Hook>()),
      home_button_(
          []() -> std::expected<void, ProviderError> {
            const Result blocked = appletBeginBlockingHomeButton(0);
            if (R_FAILED(blocked)) {
              return std::unexpected(
                  ProviderError{ProviderErrorCode::platform_failure, blocked});
            }
            return {};
          },
          []() noexcept { static_cast<void>(appletEndBlockingHomeButton()); }) {
}

HorizonLifecycle::HorizonLifecycle(HorizonLifecycle&& other) noexcept = default;

auto HorizonLifecycle::operator=(HorizonLifecycle&& other) noexcept
    -> HorizonLifecycle& = default;

HorizonLifecycle::~HorizonLifecycle() = default;

auto HorizonLifecycle::phase() const noexcept -> ScenePhase {
  switch (appletGetFocusState()) {
    case AppletFocusState_InFocus:
      return ScenePhase::active;
    case AppletFocusState_OutOfFocus:
      return ScenePhase::inactive;
    case AppletFocusState_Background:
      return ScenePhase::background;
  }
  return ScenePhase::active;
}

auto HorizonLifecycle::poll() -> std::optional<LifecycleEvent> {
  return hook_->poll();
}

auto HorizonLifecycle::block_home_button()
    -> std::expected<HomeButtonBlock, ProviderError> {
  if (!application()) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  return home_button_.acquire().transform([](support::ScopedCallback release) {
    return HomeButtonBlock{std::move(release)};
  });
}

auto HorizonLifecycle::application() noexcept -> bool {
  const AppletType type = appletGetAppletType();
  return type == AppletType_Application || type == AppletType_SystemApplication;
}

}
