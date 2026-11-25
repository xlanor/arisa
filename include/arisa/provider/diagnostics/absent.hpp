#pragma once

#include <arisa/provider/diagnostics/provider.hpp>

namespace arisa::provider {

class AbsentDiagnosticsProvider final : public DiagnosticsProvider {
 public:
  void report(const Diagnostic& diagnostic) noexcept override;
};

}
