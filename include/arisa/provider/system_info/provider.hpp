#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/system_info/dimming_inhibit.hpp>
#include <arisa/support/bounded_value.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace arisa::provider {

struct BatteryPercentTag;
struct SignalBarsTag;

using BatteryPercent =
    support::BoundedValue<std::uint8_t, 0, 100, BatteryPercentTag>;
using SignalBars = support::BoundedValue<std::uint8_t, 0, 3, SignalBarsTag>;

struct BatteryState {
  BatteryPercent percent{BatteryPercent::of<0>()};
  bool charging{false};

  friend auto operator==(const BatteryState&, const BatteryState&)
      -> bool = default;
};

enum class NetworkKind : std::uint8_t {
  none,
  wifi,
  ethernet,
};

struct NetworkState {
  NetworkKind kind{NetworkKind::none};
  SignalBars signal_bars{SignalBars::of<0>()};
  std::string address;

  friend auto operator==(const NetworkState&, const NetworkState&)
      -> bool = default;
};

class SystemInfoProvider {
 public:
  virtual ~SystemInfoProvider() = default;

  [[nodiscard]] virtual auto battery() const
      -> std::expected<BatteryState, ProviderError> = 0;

  [[nodiscard]] virtual auto network() const
      -> std::expected<NetworkState, ProviderError> = 0;

  [[nodiscard]] virtual auto inhibit_dimming()
      -> std::expected<DimmingInhibit, ProviderError> = 0;

  [[nodiscard]] virtual auto preferred_languages() const
      -> std::expected<std::vector<std::string>, ProviderError> {
    return std::unexpected(ProviderError::unsupported_capability());
  }
};

}
