#pragma once

#include <arisa/conformance/conformance_options.hpp>
#include <arisa/conformance/report.hpp>
#include <arisa/provider/factory.hpp>

namespace arisa::conformance {

class ConformanceSuite {
 public:
  [[nodiscard]] static auto run(provider::ProviderFactory& factory,
                                const ConformanceOptions& options = {})
      -> Report;

 private:
  static void check_identity(provider::ProviderFactory& factory,
                             Report& report);
};

}
