#include <arisa/provider/applets/absent.hpp>

namespace arisa::provider {

auto AbsentAppletProvider::select_account()
    -> std::expected<std::optional<UserAccount>, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

auto AbsentAppletProvider::select_players(PlayerSelection)
    -> std::expected<std::uint8_t, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

auto AbsentAppletProvider::show_error(const ErrorReport&)
    -> std::expected<void, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

}
