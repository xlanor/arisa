#pragma once

#include <arisa/conformance/report.hpp>
#include <arisa/provider/factory.hpp>

namespace arisa::conformance {

class AudioSuite {
 public:
  static void run(provider::ProviderFactory& factory, Report& report);

 private:
  static void check_roles(provider::AudioProvider& audio, bool supplied,
                          Report& report);
};

}
