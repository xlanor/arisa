#pragma once

#include <arisa/provider/applets/provider.hpp>
#include <arisa/provider/horizon/service_session.hpp>

#include <switch.h>

#include <string>

namespace arisa::provider::horizon {

class HorizonApplets final : public AppletProvider {
 public:
  static constexpr std::uint32_t cancelled_module = 124;
  static constexpr std::uint32_t cancelled_description = 1;

  [[nodiscard]] static auto create()
      -> std::expected<HorizonApplets, ProviderError>;

  [[nodiscard]] auto select_account()
      -> std::expected<std::optional<UserAccount>, ProviderError> override;

  [[nodiscard]] auto select_players(PlayerSelection request)
      -> std::expected<std::uint8_t, ProviderError> override;

  [[nodiscard]] auto show_error(const ErrorReport& report)
      -> std::expected<void, ProviderError> override;

 private:
  struct AccountService {
    static auto open() noexcept -> Result {
      return accountInitialize(AccountServiceType_Application);
    }

    static void close() noexcept { accountExit(); }
  };

  explicit HorizonApplets(ServiceSession<AccountService> accounts) noexcept
      : accounts_(std::move(accounts)) {}

  static auto identifier(const AccountUid& uid) -> std::string;

  static auto nickname(const AccountUid& uid)
      -> std::expected<std::string, ProviderError>;

  ServiceSession<AccountService> accounts_;
};

}
