#pragma once

#include <arisa/provider/diagnostics/provider.hpp>

#include <mutex>
#include <vector>

namespace arisa::provider::headless {

class RecordingDiagnostics final : public DiagnosticsProvider {
 public:
  void report(const Diagnostic& diagnostic) noexcept override;

  auto snapshot() const -> std::vector<Diagnostic>;

  auto take() -> std::vector<Diagnostic>;

 private:
  mutable std::mutex mutex_;
  std::vector<Diagnostic> recorded_;
};

}
