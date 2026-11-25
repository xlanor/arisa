#include <arisa/conformance/report.hpp>

#include <gtest/gtest.h>

namespace arisa::conformance {

TEST(ReportTest, EmptyReportPasses) {
  const Report report;
  EXPECT_TRUE(report.passed());
  EXPECT_EQ(report.size(), 0U);
}

TEST(ReportTest, AnyFailureFailsTheReport) {
  Report report;
  report.record("a", true, "ignored when passing");
  report.record("b", false, "why b failed");
  EXPECT_FALSE(report.passed());
  EXPECT_EQ(report.failure_count(), 1U);
  ASSERT_EQ(report.results().size(), 2U);
  EXPECT_EQ(report.results()[0].detail, "");
  EXPECT_EQ(report.results()[1].detail, "why b failed");
  int visited = 0;
  for (const CheckResult& result : report) {
    static_cast<void>(result);
    ++visited;
  }
  EXPECT_EQ(visited, 2);
}

}
