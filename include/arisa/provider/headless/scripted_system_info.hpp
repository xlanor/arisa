#pragma once

#include <arisa/provider/system_info/provider.hpp>

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace arisa::provider::headless {

class ScriptedSystemInfo final : public SystemInfoProvider {
 public:
  [[nodiscard]] auto battery() const
      -> std::expected<BatteryState, ProviderError> override;

  [[nodiscard]] auto network() const
      -> std::expected<NetworkState, ProviderError> override;

  [[nodiscard]] auto inhibit_dimming()
      -> std::expected<DimmingInhibit, ProviderError> override;

  [[nodiscard]] auto preferred_languages() const
      -> std::expected<std::vector<std::string>, ProviderError> override;

  void set_languages(std::vector<std::string> languages);

  void set_battery(BatteryState battery);

  void set_network(NetworkState network);

  auto live_dimming_inhibits() const noexcept -> std::size_t;

 private:
  mutable std::mutex mutex_;
  BatteryState battery_{BatteryPercent::of<100>(), false};
  NetworkState network_;
  std::vector<std::string> languages_;
  std::shared_ptr<std::atomic<std::size_t>> live_inhibits_{
      std::make_shared<std::atomic<std::size_t>>(0)};
};

}
