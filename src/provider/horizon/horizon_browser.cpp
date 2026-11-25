#include <arisa/provider/horizon/horizon_browser.hpp>

#include <switch.h>

#include <string>

namespace arisa::provider::horizon {

auto HorizonBrowser::create() -> std::expected<HorizonBrowser, ProviderError> {
  const AppletType type = appletGetAppletType();
  if (type != AppletType_Application && type != AppletType_SystemApplication) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  return HorizonBrowser{};
}

auto HorizonBrowser::open(const Url& url)
    -> std::expected<void, ProviderError> {
  if (!url.secure()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  const std::string address{url.value()};
  WebCommonConfig config{};
  Result result = webPageCreate(&config, address.c_str());
  if (R_SUCCEEDED(result)) {
    result = webConfigSetWhitelist(&config, "^https://");
  }
  if (R_SUCCEEDED(result)) {
    result = webConfigShow(&config, nullptr);
  }
  if (R_FAILED(result)) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::platform_failure, result});
  }
  return {};
}

}
