#pragma once

#include <arisa/conformance/report.hpp>
#include <arisa/provider/factory.hpp>

namespace arisa::conformance {

class SystemInfoSuite {
 public:
  static void run(provider::ProviderFactory& factory, Report& report);

 private:
  static void check_supplied(provider::SystemInfoProvider& system_info,
                             Report& report);
  static void check_absent(provider::SystemInfoProvider& system_info,
                           Report& report);
};

}
