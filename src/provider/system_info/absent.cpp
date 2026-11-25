#include <arisa/provider/system_info/absent.hpp>

namespace arisa::provider {

auto AbsentSystemInfoProvider::battery() const
    -> std::expected<BatteryState, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

auto AbsentSystemInfoProvider::network() const
    -> std::expected<NetworkState, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

auto AbsentSystemInfoProvider::inhibit_dimming()
    -> std::expected<DimmingInhibit, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

}
