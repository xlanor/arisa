#pragma once

#include <arisa/conformance/conformance_options.hpp>
#include <arisa/conformance/report.hpp>
#include <arisa/provider/factory.hpp>

namespace arisa::conformance {

class TextInputSuite {
 public:
  static void run(provider::ProviderFactory& factory,
                  const ConformanceOptions& options, Report& report);

 private:
  static void check_absent(provider::TextInputProvider& text_input,
                           Report& report);
  static void check_begin_and_cancel(provider::TextInputProvider& text_input,
                                     Report& report);
  static void check_limit_rules(Report& report);
};

}
