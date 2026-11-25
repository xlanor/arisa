#pragma once

#include <arisa/provider/applets/provider.hpp>

#include <mutex>
#include <optional>
#include <vector>

namespace arisa::provider::headless {

class ScriptedApplets final : public AppletProvider {
 public:
  [[nodiscard]] auto select_account()
      -> std::expected<std::optional<UserAccount>, ProviderError> override;

  [[nodiscard]] auto select_players(PlayerSelection request)
      -> std::expected<std::uint8_t, ProviderError> override;

  [[nodiscard]] auto show_error(const ErrorReport& report)
      -> std::expected<void, ProviderError> override;

  void set_account(std::optional<UserAccount> account);

  void set_players(std::uint8_t players);

  auto errors() const -> std::vector<ErrorReport>;

  auto selections() const -> std::vector<PlayerSelection>;

 private:
  mutable std::mutex mutex_;
  std::optional<UserAccount> account_;
  std::uint8_t players_{1};
  std::vector<ErrorReport> errors_;
  std::vector<PlayerSelection> selections_;
};

}
