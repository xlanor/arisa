#include <arisa/conformance/system_info_suite.hpp>

namespace arisa::conformance {

using provider::Capability;
using provider::NetworkKind;
using provider::ProviderErrorCode;

void SystemInfoSuite::run(provider::ProviderFactory& factory, Report& report) {
  provider::SystemInfoProvider& system_info = factory.system_info();
  if (factory.supplies(Capability::system_info)) {
    check_supplied(system_info, report);
  } else {
    check_absent(system_info, report);
  }
}

void SystemInfoSuite::check_supplied(provider::SystemInfoProvider& system_info,
                                     Report& report) {
  const auto unsupported = [](const provider::ProviderError& error) {
    return error.code() == ProviderErrorCode::unsupported_capability;
  };
  const auto battery = system_info.battery();
  report.record("system_info.battery",
                battery.has_value() || !unsupported(battery.error()),
                "battery() reported unsupported_capability while supplied");
  const auto network = system_info.network();
  const bool network_valid = !network.has_value() ||
                             network->kind == NetworkKind::none ||
                             network->kind == NetworkKind::wifi ||
                             network->kind == NetworkKind::ethernet;
  report.record("system_info.network", network_valid,
                "network() returned an invalid kind");
  bool inhibit_valid = false;
  {
    const auto inhibit = system_info.inhibit_dimming();
    inhibit_valid = !inhibit.has_value() || inhibit->active();
  }
  report.record("system_info.dimming_inhibit", inhibit_valid,
                "inhibit_dimming() returned an inactive scope");
}

void SystemInfoSuite::check_absent(provider::SystemInfoProvider& system_info,
                                   Report& report) {
  const auto unsupported = [](const auto& result) {
    return !result.has_value() &&
           result.error().code() == ProviderErrorCode::unsupported_capability;
  };
  report.record("system_info.absent",
                unsupported(system_info.battery()) &&
                    unsupported(system_info.network()) &&
                    unsupported(system_info.inhibit_dimming()),
                "an unsupplied system info provider did not behave as absent");
}

}
