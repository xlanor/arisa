#pragma once

#include <cstdint>
#include <string>

namespace arisa::provider {

enum class Severity : std::uint8_t {
  debug,
  info,
  warning,
  error,
};

struct Diagnostic {
  Severity severity{Severity::info};
  std::string category;
  std::string message;

  friend auto operator==(const Diagnostic&, const Diagnostic&)
      -> bool = default;
};

class DiagnosticsProvider {
 public:
  virtual ~DiagnosticsProvider() = default;

  virtual void report(const Diagnostic& diagnostic) noexcept = 0;
};

}
