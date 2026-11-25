#pragma once

#include <arisa/conformance/report.hpp>
#include <arisa/provider/factory.hpp>

namespace arisa::conformance {

class DiagnosticsSuite {
 public:
  static void run(provider::ProviderFactory& factory, Report& report);

 private:
  static void check_concurrent_reports(provider::ProviderFactory& factory,
                                       Report& report);
};

}
