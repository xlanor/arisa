#pragma once

#include <arisa/provider/system_info/provider.hpp>

namespace arisa::provider {

class AbsentSystemInfoProvider final : public SystemInfoProvider {
 public:
  [[nodiscard]] auto battery() const
      -> std::expected<BatteryState, ProviderError> override;
  [[nodiscard]] auto network() const
      -> std::expected<NetworkState, ProviderError> override;
  [[nodiscard]] auto inhibit_dimming()
      -> std::expected<DimmingInhibit, ProviderError> override;
};

}
