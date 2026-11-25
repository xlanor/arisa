#pragma once

#include <arisa/conformance/report.hpp>
#include <arisa/provider/factory.hpp>

namespace arisa::conformance {

class HapticsSuite {
 public:
  static void run(provider::ProviderFactory& factory, Report& report);

 private:
  static void check_patterns(provider::HapticsProvider& haptics, bool supplied,
                             Report& report);
};

}
