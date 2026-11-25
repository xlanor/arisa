#include <arisa/provider/headless/scripted_lifecycle.hpp>

namespace arisa::provider::headless {

auto ScriptedLifecycle::phase() const noexcept -> ScenePhase {
  return phase_.load();
}

auto ScriptedLifecycle::poll() -> std::optional<LifecycleEvent> {
  const std::scoped_lock lock{mutex_};
  if (pending_.empty()) {
    return std::nullopt;
  }
  const LifecycleEvent event = pending_.front();
  pending_.pop_front();
  return event;
}

auto ScriptedLifecycle::block_home_button()
    -> std::expected<HomeButtonBlock, ProviderError> {
  live_blocks_->fetch_add(1);
  return HomeButtonBlock{support::ScopedCallback{
      [live = live_blocks_]() noexcept { live->fetch_sub(1); }}};
}

void ScriptedLifecycle::push(LifecycleEvent event) {
  const std::scoped_lock lock{mutex_};
  pending_.push_back(event);
}

void ScriptedLifecycle::set_phase(ScenePhase phase) noexcept {
  phase_.store(phase);
}

auto ScriptedLifecycle::live_home_button_blocks() const noexcept
    -> std::size_t {
  return live_blocks_->load();
}

}
