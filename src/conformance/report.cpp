#include <arisa/conformance/report.hpp>

#include <algorithm>
#include <format>
#include <utility>

namespace arisa::conformance {

void Report::trace(std::string_view message) const {
  if (trace_) {
    trace_(message);
  }
}

void Report::record(std::string name, bool passed, std::string detail) {
  trace(std::format("{} {}", passed ? "PASS" : "FAIL", name));
  results_.push_back(
      CheckResult{.name = std::move(name),
                  .passed = passed,
                  .detail = passed ? std::string{} : std::move(detail)});
}

auto Report::passed() const noexcept -> bool { return failure_count() == 0; }

auto Report::failure_count() const noexcept -> std::size_t {
  return static_cast<std::size_t>(std::ranges::count_if(
      results_, [](const CheckResult& result) { return !result.passed; }));
}

}
