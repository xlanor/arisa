#include <arisa/provider/standard/stderr_diagnostics.hpp>

#include <cstdio>
#include <print>

namespace arisa::provider::standard {

StderrDiagnostics::StderrDiagnostics(Severity threshold) noexcept
    : threshold_(threshold) {}

void StderrDiagnostics::report(const Diagnostic& diagnostic) noexcept {
  if (diagnostic.severity < threshold_) {
    return;
  }
  const std::scoped_lock lock{mutex_};
  std::println(stderr, "[{}] {}: {}", label(diagnostic.severity),
               diagnostic.category, diagnostic.message);
}

auto StderrDiagnostics::label(Severity severity) noexcept -> std::string_view {
  switch (severity) {
    case Severity::debug:
      return "debug";
    case Severity::info:
      return "info";
    case Severity::warning:
      return "warning";
    case Severity::error:
      return "error";
  }
  return "unknown";
}

}
