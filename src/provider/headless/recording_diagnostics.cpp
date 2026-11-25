#include <arisa/provider/headless/recording_diagnostics.hpp>

#include <utility>

namespace arisa::provider::headless {

void RecordingDiagnostics::report(const Diagnostic& diagnostic) noexcept {
  const std::scoped_lock lock{mutex_};
  recorded_.push_back(diagnostic);
}

auto RecordingDiagnostics::snapshot() const -> std::vector<Diagnostic> {
  const std::scoped_lock lock{mutex_};
  return recorded_;
}

auto RecordingDiagnostics::take() -> std::vector<Diagnostic> {
  const std::scoped_lock lock{mutex_};
  return std::exchange(recorded_, {});
}

}
