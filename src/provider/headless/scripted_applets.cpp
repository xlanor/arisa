#include <arisa/provider/headless/scripted_applets.hpp>

#include <algorithm>
#include <utility>

namespace arisa::provider::headless {

auto ScriptedApplets::select_account()
    -> std::expected<std::optional<UserAccount>, ProviderError> {
  const std::scoped_lock lock{mutex_};
  return account_;
}

auto ScriptedApplets::select_players(PlayerSelection request)
    -> std::expected<std::uint8_t, ProviderError> {
  const std::scoped_lock lock{mutex_};
  selections_.push_back(request);
  return std::clamp(players_, request.minimum, request.maximum);
}

auto ScriptedApplets::show_error(const ErrorReport& report)
    -> std::expected<void, ProviderError> {
  const std::scoped_lock lock{mutex_};
  errors_.push_back(report);
  return {};
}

void ScriptedApplets::set_account(std::optional<UserAccount> account) {
  const std::scoped_lock lock{mutex_};
  account_ = std::move(account);
}

void ScriptedApplets::set_players(std::uint8_t players) {
  const std::scoped_lock lock{mutex_};
  players_ = players;
}

auto ScriptedApplets::errors() const -> std::vector<ErrorReport> {
  const std::scoped_lock lock{mutex_};
  return errors_;
}

auto ScriptedApplets::selections() const -> std::vector<PlayerSelection> {
  const std::scoped_lock lock{mutex_};
  return selections_;
}

}
