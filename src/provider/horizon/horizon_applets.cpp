#include <arisa/provider/horizon/horizon_applets.hpp>

#include <algorithm>
#include <format>
#include <utility>

namespace arisa::provider::horizon {

auto HorizonApplets::create() -> std::expected<HorizonApplets, ProviderError> {
  const AppletType type = appletGetAppletType();
  if (type != AppletType_Application && type != AppletType_SystemApplication) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  auto accounts = ServiceSession<AccountService>::create();
  if (!accounts.has_value()) {
    return std::unexpected(accounts.error());
  }
  return HorizonApplets{std::move(*accounts)};
}

auto HorizonApplets::identifier(const AccountUid& uid) -> std::string {
  return std::format("{:016x}{:016x}", uid.uid[1], uid.uid[0]);
}

auto HorizonApplets::nickname(const AccountUid& uid)
    -> std::expected<std::string, ProviderError> {
  AccountProfile profile{};
  Result result = accountGetProfile(&profile, uid);
  if (R_FAILED(result)) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::platform_failure, result});
  }
  AccountProfileBase base{};
  result = accountProfileGet(&profile, nullptr, &base);
  accountProfileClose(&profile);
  if (R_FAILED(result)) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::platform_failure, result});
  }
  const std::size_t length =
      std::ranges::find(base.nickname, '\0') - std::begin(base.nickname);
  return std::string{base.nickname, length};
}

auto HorizonApplets::select_account()
    -> std::expected<std::optional<UserAccount>, ProviderError> {
  AccountUid uid{};
  PselUserSelectionSettings settings{};
  const Result shown = pselShowUserSelector(&uid, &settings);
  if (R_FAILED(shown)) {
    if (R_MODULE(shown) == cancelled_module &&
        R_DESCRIPTION(shown) == cancelled_description) {
      return std::optional<UserAccount>{};
    }
    return std::unexpected(
        ProviderError{ProviderErrorCode::platform_failure, shown});
  }
  if (!accountUidIsValid(&uid)) {
    return std::optional<UserAccount>{};
  }
  auto name = nickname(uid);
  if (!name.has_value()) {
    return std::unexpected(name.error());
  }
  return std::optional<UserAccount>{
      UserAccount{.identifier = identifier(uid), .nickname = std::move(*name)}};
}

auto HorizonApplets::select_players(PlayerSelection request)
    -> std::expected<std::uint8_t, ProviderError> {
  HidLaControllerSupportArg argument{};
  hidLaCreateControllerSupportArg(&argument);
  argument.hdr.player_count_min = static_cast<s8>(request.minimum);
  argument.hdr.player_count_max = static_cast<s8>(request.maximum);
  argument.hdr.enable_single_mode = request.single_player ? 1 : 0;
  HidLaControllerSupportResultInfo info{};
  const Result shown = hidLaShowControllerSupport(&info, &argument);
  if (R_FAILED(shown)) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::platform_failure, shown});
  }
  return static_cast<std::uint8_t>(std::max<s8>(info.player_count, 0));
}

auto HorizonApplets::show_error(const ErrorReport& report)
    -> std::expected<void, ProviderError> {
  ErrorApplicationConfig config{};
  Result result = errorApplicationCreate(
      &config, report.message.c_str(),
      report.details.empty() ? nullptr : report.details.c_str());
  if (R_SUCCEEDED(result)) {
    errorApplicationSetNumber(&config, report.code);
    result = errorApplicationShow(&config);
  }
  if (R_FAILED(result)) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::platform_failure, result});
  }
  return {};
}

}
