#include <arisa/conformance/clipboard_suite.hpp>

#include <string>

namespace arisa::conformance {

using provider::ProviderErrorCode;

void ClipboardSuite::run(provider::ProviderFactory& factory, Report& report) {
  provider::ClipboardProvider& clipboard = factory.clipboard();
  if (factory.supplies(provider::Capability::clipboard)) {
    check_round_trip(clipboard, report);
  } else {
    check_absent(clipboard, report);
  }
}

void ClipboardSuite::check_round_trip(provider::ClipboardProvider& clipboard,
                                      Report& report) {
  const auto previous = clipboard.read_text();
  const bool written = clipboard.write_text(probe_text).has_value();
  const auto read_back = clipboard.read_text();
  if (previous.has_value()) {
    static_cast<void>(clipboard.write_text(*previous));
  }
  report.record("clipboard.round_trip",
                written && read_back.has_value() && *read_back == probe_text,
                "text written to the clipboard did not read back");
}

void ClipboardSuite::check_absent(provider::ClipboardProvider& clipboard,
                                  Report& report) {
  const auto read = clipboard.read_text();
  const auto written = clipboard.write_text(probe_text);
  report.record(
      "clipboard.absent",
      !read.has_value() &&
          read.error().code() == ProviderErrorCode::unsupported_capability &&
          !written.has_value() &&
          written.error().code() == ProviderErrorCode::unsupported_capability,
      "an unsupplied clipboard provider did not return "
      "unsupported_capability");
}

}
