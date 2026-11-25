#include <arisa/provider/headless/scripted_system_info.hpp>

#include <utility>

namespace arisa::provider::headless {

auto ScriptedSystemInfo::battery() const
    -> std::expected<BatteryState, ProviderError> {
  const std::scoped_lock lock{mutex_};
  return battery_;
}

auto ScriptedSystemInfo::network() const
    -> std::expected<NetworkState, ProviderError> {
  const std::scoped_lock lock{mutex_};
  return network_;
}

auto ScriptedSystemInfo::inhibit_dimming()
    -> std::expected<DimmingInhibit, ProviderError> {
  live_inhibits_->fetch_add(1);
  return DimmingInhibit{support::ScopedCallback{
      [live = live_inhibits_]() noexcept { live->fetch_sub(1); }}};
}

auto ScriptedSystemInfo::preferred_languages() const
    -> std::expected<std::vector<std::string>, ProviderError> {
  const std::scoped_lock lock{mutex_};
  if (languages_.empty()) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  return languages_;
}

void ScriptedSystemInfo::set_languages(std::vector<std::string> languages) {
  const std::scoped_lock lock{mutex_};
  languages_ = std::move(languages);
}

void ScriptedSystemInfo::set_battery(BatteryState battery) {
  const std::scoped_lock lock{mutex_};
  battery_ = battery;
}

void ScriptedSystemInfo::set_network(NetworkState network) {
  const std::scoped_lock lock{mutex_};
  network_ = std::move(network);
}

auto ScriptedSystemInfo::live_dimming_inhibits() const noexcept -> std::size_t {
  return live_inhibits_->load();
}

}
