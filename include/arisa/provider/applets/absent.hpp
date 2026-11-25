#pragma once

#include <arisa/provider/applets/provider.hpp>

namespace arisa::provider {

class AbsentAppletProvider final : public AppletProvider {
 public:
  [[nodiscard]] auto select_account()
      -> std::expected<std::optional<UserAccount>, ProviderError> override;

  [[nodiscard]] auto select_players(PlayerSelection request)
      -> std::expected<std::uint8_t, ProviderError> override;

  [[nodiscard]] auto show_error(const ErrorReport& report)
      -> std::expected<void, ProviderError> override;
};

}
