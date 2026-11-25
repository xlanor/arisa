#include <arisa/conformance/browser_suite.hpp>

#include <algorithm>
#include <array>

namespace arisa::conformance {

using provider::ProviderErrorCode;
using provider::Url;

void BrowserSuite::run(provider::ProviderFactory& factory,
                       const ConformanceOptions& options, Report& report) {
  const bool supplied = factory.supplies(provider::Capability::browser);
  if (!supplied || options.open_external) {
    check_open(factory.browser(), supplied, report);
  }
  check_url_rules(report);
}

void BrowserSuite::check_open(provider::BrowserProvider& browser, bool supplied,
                              Report& report) {
  const auto url = Url::create(probe_url);
  if (!url.has_value()) {
    report.record("browser.open", false, "the probe URL was rejected");
    return;
  }
  const auto opened = browser.open(*url);
  const bool unsupported =
      !opened.has_value() &&
      opened.error().code() == ProviderErrorCode::unsupported_capability;
  report.record(supplied ? "browser.open" : "browser.absent",
                supplied != unsupported,
                supplied ? "open() reported unsupported_capability while "
                           "supplied"
                         : "an unsupplied browser provider did not return "
                           "unsupported_capability");
}

void BrowserSuite::check_url_rules(Report& report) {
  static constexpr std::array rejected{
      std::string_view{""},
      std::string_view{"ftp://example.com"},
      std::string_view{"https://"},
      std::string_view{"https:///path"},
      std::string_view{"https://exa mple.com"},
      std::string_view{"javascript:alert(1)"},
      std::string_view{"example.com"},
  };
  const bool all_rejected =
      std::ranges::none_of(rejected, [](std::string_view candidate) {
        return Url::create(candidate).has_value();
      });
  report.record("browser.url_rules",
                all_rejected && Url::create("http://example.com").has_value() &&
                    Url::create("HTTPS://example.com/a?b#c").has_value(),
                "Url accepted a non-http(s) URL or rejected a valid one");
}

}
