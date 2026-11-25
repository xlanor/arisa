#pragma once

#include <arisa/conformance/conformance_options.hpp>
#include <arisa/conformance/report.hpp>
#include <arisa/provider/factory.hpp>

#include <string_view>

namespace arisa::conformance {

class BrowserSuite {
 public:
  static constexpr std::string_view probe_url =
      "https://example.com/arisa-conformance";

  static void run(provider::ProviderFactory& factory,
                  const ConformanceOptions& options, Report& report);

 private:
  static void check_open(provider::BrowserProvider& browser, bool supplied,
                         Report& report);
  static void check_url_rules(Report& report);
};

}
