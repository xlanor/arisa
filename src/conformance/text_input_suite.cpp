#include <arisa/conformance/text_input_suite.hpp>

namespace arisa::conformance {

using provider::ProviderErrorCode;
using provider::TextInputLimit;
using provider::TextInputRequest;

void TextInputSuite::run(provider::ProviderFactory& factory,
                         const ConformanceOptions& options, Report& report) {
  provider::TextInputProvider& text_input = factory.text_input();
  if (!factory.supplies(provider::Capability::text_input)) {
    check_absent(text_input, report);
  } else if (options.open_external) {
    check_begin_and_cancel(text_input, report);
  }
  check_limit_rules(report);
}

void TextInputSuite::check_absent(provider::TextInputProvider& text_input,
                                  Report& report) {
  const auto session = text_input.begin(TextInputRequest{});
  report.record(
      "text_input.absent",
      !session.has_value() &&
          session.error().code() == ProviderErrorCode::unsupported_capability,
      "an unsupplied text input provider did not return "
      "unsupported_capability");
}

void TextInputSuite::check_begin_and_cancel(
    provider::TextInputProvider& text_input, Report& report) {
  bool began = false;
  bool unsupported = false;
  {
    auto session = text_input.begin(
        TextInputRequest{.initial = "arisa", .hint = "arisa conformance"});
    began = session.has_value();
    unsupported = !began && session.error().code() ==
                                ProviderErrorCode::unsupported_capability;
    if (began) {
      static_cast<void>(session->poll());
    }
  }
  report.record("text_input.begin_and_cancel", began && !unsupported,
                "a supplied text input provider could not begin a session");
}

void TextInputSuite::check_limit_rules(Report& report) {
  report.record("text_input.limit_rules",
                !TextInputLimit::create(0).has_value() &&
                    !TextInputLimit::create(4097).has_value() &&
                    TextInputLimit::create(1).has_value() &&
                    TextInputLimit::create(4096).has_value(),
                "TextInputLimit accepted a length outside 1 to 4096");
}

}
