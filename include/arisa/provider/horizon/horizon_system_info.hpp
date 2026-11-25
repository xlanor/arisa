#pragma once

#include <arisa/provider/counted_scope.hpp>
#include <arisa/provider/horizon/service_session.hpp>
#include <arisa/provider/system_info/provider.hpp>

#include <switch.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace arisa::provider::horizon {

class HorizonSystemInfo final : public SystemInfoProvider {
 public:
  [[nodiscard]] static auto create()
      -> std::expected<HorizonSystemInfo, ProviderError>;

  [[nodiscard]] auto battery() const
      -> std::expected<BatteryState, ProviderError> override;

  [[nodiscard]] auto network() const
      -> std::expected<NetworkState, ProviderError> override;

  [[nodiscard]] auto inhibit_dimming()
      -> std::expected<DimmingInhibit, ProviderError> override;

  [[nodiscard]] auto preferred_languages() const
      -> std::expected<std::vector<std::string>, ProviderError> override;

 private:
  struct PsmService {
    static auto open() noexcept -> Result { return psmInitialize(); }

    static void close() noexcept { psmExit(); }
  };

  struct SetService {
    static auto open() noexcept -> Result { return setInitialize(); }

    static void close() noexcept { setExit(); }
  };

  struct NifmService {
    static auto open() noexcept -> Result {
      return nifmInitialize(NifmServiceType_User);
    }

    static void close() noexcept { nifmExit(); }
  };

  HorizonSystemInfo(ServiceSession<PsmService> psm,
                    ServiceSession<NifmService> nifm,
                    std::optional<ServiceSession<SetService>> settings);

  static auto dotted(std::uint32_t address) -> std::string;

  ServiceSession<PsmService> psm_;
  ServiceSession<NifmService> nifm_;
  std::optional<ServiceSession<SetService>> settings_;
  CountedScope media_playback_;
};

}
