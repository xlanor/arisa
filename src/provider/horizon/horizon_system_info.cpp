#include <arisa/provider/horizon/horizon_system_info.hpp>

#include <algorithm>
#include <format>
#include <utility>

namespace arisa::provider::horizon {

HorizonSystemInfo::HorizonSystemInfo(
    ServiceSession<PsmService> psm, ServiceSession<NifmService> nifm,
    std::optional<ServiceSession<SetService>> settings)
    : psm_(std::move(psm)),
      nifm_(std::move(nifm)),
      settings_(std::move(settings)),
      media_playback_(
          []() -> std::expected<void, ProviderError> {
            const Result set = appletSetMediaPlaybackState(true);
            if (R_FAILED(set)) {
              return std::unexpected(
                  ProviderError{ProviderErrorCode::platform_failure, set});
            }
            return {};
          },
          []() noexcept {
            static_cast<void>(appletSetMediaPlaybackState(false));
          }) {}

auto HorizonSystemInfo::create()
    -> std::expected<HorizonSystemInfo, ProviderError> {
  auto psm = ServiceSession<PsmService>::create();
  if (!psm.has_value()) {
    return std::unexpected(psm.error());
  }
  auto nifm = ServiceSession<NifmService>::create();
  if (!nifm.has_value()) {
    return std::unexpected(nifm.error());
  }
  auto settings = ServiceSession<SetService>::create();
  return HorizonSystemInfo{
      std::move(*psm), std::move(*nifm),
      settings.has_value()
          ? std::optional<ServiceSession<SetService>>{std::move(*settings)}
          : std::nullopt};
}

auto HorizonSystemInfo::preferred_languages() const
    -> std::expected<std::vector<std::string>, ProviderError> {
  if (!settings_.has_value()) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  u64 code = 0;
  const Result fetched = setGetSystemLanguage(&code);
  if (R_FAILED(fetched)) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::platform_failure, fetched});
  }
  std::string tag;
  for (std::size_t index = 0; index < sizeof(code); ++index) {
    const auto character = static_cast<char>((code >> (index * 8U)) & 0xFFU);
    if (character == '\0') {
      break;
    }
    tag.push_back(character);
  }
  if (tag.empty()) {
    return std::unexpected(ProviderError{ProviderErrorCode::platform_failure});
  }
  return std::vector<std::string>{std::move(tag)};
}

auto HorizonSystemInfo::battery() const
    -> std::expected<BatteryState, ProviderError> {
  u32 percent = 0;
  const Result charge = psmGetBatteryChargePercentage(&percent);
  if (R_FAILED(charge)) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::platform_failure, charge});
  }
  PsmChargerType charger = PsmChargerType_Unconnected;
  const Result typed = psmGetChargerType(&charger);
  if (R_FAILED(typed)) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::platform_failure, typed});
  }
  const auto clamped = BatteryPercent::create(static_cast<std::uint8_t>(
      std::min<u32>(percent, BatteryPercent::maximum())));
  return BatteryState{.percent = clamped.value_or(BatteryPercent::of<0>()),
                      .charging = charger == PsmChargerType_EnoughPower ||
                                  charger == PsmChargerType_LowPower};
}

auto HorizonSystemInfo::network() const
    -> std::expected<NetworkState, ProviderError> {
  NifmInternetConnectionType type{};
  u32 strength = 0;
  NifmInternetConnectionStatus status{};
  if (R_FAILED(nifmGetInternetConnectionStatus(&type, &strength, &status)) ||
      status != NifmInternetConnectionStatus_Connected) {
    return NetworkState{};
  }
  u32 address = 0;
  const std::string text = R_SUCCEEDED(nifmGetCurrentIpAddress(&address))
                               ? dotted(address)
                               : std::string{};
  if (type == NifmInternetConnectionType_Ethernet) {
    return NetworkState{.kind = NetworkKind::ethernet,
                        .signal_bars = SignalBars::of<3>(),
                        .address = text};
  }
  const auto bars = SignalBars::create(static_cast<std::uint8_t>(
      std::min<u32>(strength, SignalBars::maximum())));
  return NetworkState{.kind = NetworkKind::wifi,
                      .signal_bars = bars.value_or(SignalBars::of<0>()),
                      .address = text};
}

auto HorizonSystemInfo::inhibit_dimming()
    -> std::expected<DimmingInhibit, ProviderError> {
  return media_playback_.acquire().transform(
      [](support::ScopedCallback release) {
        return DimmingInhibit{std::move(release)};
      });
}

auto HorizonSystemInfo::dotted(std::uint32_t address) -> std::string {
  return std::format("{}.{}.{}.{}", address & 0xFFU, (address >> 8U) & 0xFFU,
                     (address >> 16U) & 0xFFU, (address >> 24U) & 0xFFU);
}

}
