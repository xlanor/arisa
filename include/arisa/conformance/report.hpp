#pragma once

#include <arisa/conformance/conformance_options.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::conformance {

struct CheckResult {
  std::string name;
  bool passed{false};
  std::string detail;
};

class Report {
 public:
  Report() = default;

  explicit Report(ConformanceTrace trace) : trace_(std::move(trace)) {}

  void trace(std::string_view message) const;

  void record(std::string name, bool passed, std::string detail = {});

  auto passed() const noexcept -> bool;

  auto failure_count() const noexcept -> std::size_t;

  auto results() const noexcept -> std::span<const CheckResult> {
    return results_;
  }

  auto begin() const noexcept { return results_.begin(); }

  auto end() const noexcept { return results_.end(); }

  auto size() const noexcept -> std::size_t { return results_.size(); }

 private:
  ConformanceTrace trace_;
  std::vector<CheckResult> results_;
};

}
