#include <arisa/provider/standard/stderr_diagnostics.hpp>

#include <gtest/gtest.h>

namespace arisa::provider::standard {

TEST(StderrDiagnosticsTest, PrintsAtOrAboveThreshold) {
  StderrDiagnostics diagnostics{Severity::warning};
  EXPECT_EQ(diagnostics.threshold(), Severity::warning);
  ::testing::internal::CaptureStderr();
  diagnostics.report(Diagnostic{.severity = Severity::info,
                                .category = "arisa.test",
                                .message = "hidden"});
  diagnostics.report(Diagnostic{.severity = Severity::error,
                                .category = "arisa.test",
                                .message = "shown"});
  const std::string output = ::testing::internal::GetCapturedStderr();
  EXPECT_EQ(output, "[error] arisa.test: shown\n");
}

}
