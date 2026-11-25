#pragma once

#include <arisa/provider/diagnostics/provider.hpp>

#include <mutex>
#include <string_view>

namespace arisa::provider::standard {

class StderrDiagnostics final : public DiagnosticsProvider {
 public:
  explicit StderrDiagnostics(Severity threshold = Severity::info) noexcept;

  void report(const Diagnostic& diagnostic) noexcept override;

  auto threshold() const noexcept -> Severity { return threshold_; }

 private:
  static auto label(Severity severity) noexcept -> std::string_view;

  Severity threshold_;
  std::mutex mutex_;
};

}
