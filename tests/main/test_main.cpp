#include <gtest/gtest.h>

#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <format>
#include <fstream>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace arisa::testing {

class TimeBudget final : public ::testing::EmptyTestEventListener {
 public:
  TimeBudget()
      : limit_(configured_limit()), log_(variable("ARISA_TEST_TIMES")) {}

  void OnTestStart(const ::testing::TestInfo&) override {
    started_ = std::clock();
  }

  void OnTestEnd(const ::testing::TestInfo& info) override {
    const std::chrono::milliseconds wall{info.result()->elapsed_time()};
    const std::chrono::milliseconds spent = cpu_since(started_);
    const std::string name =
        std::format("{}.{}", info.test_suite_name(), info.name());
    if (log_.has_value()) {
      std::ofstream{*log_, std::ios::app}
          << std::format("{} {} {}\n", spent.count(), wall.count(), name);
    }
    if (limit_.count() > 0 && spent > limit_) {
      over_.push_back(name);
      std::println(stderr,
                   "[ OVER BUDGET ] {} used {} ms of cpu ({} ms wall), budget "
                   "{} ms",
                   name, spent.count(), wall.count(), limit_.count());
    }
  }

  auto exceeded() const noexcept -> bool { return !over_.empty(); }

 private:
#if defined(__SANITIZE_ADDRESS__)
  static constexpr std::chrono::milliseconds default_limit{15000};
#else
  static constexpr std::chrono::milliseconds default_limit{5000};
#endif

  static auto cpu_since(std::clock_t start) -> std::chrono::milliseconds {
    const std::clock_t now = std::clock();
    if (start == static_cast<std::clock_t>(-1) ||
        now == static_cast<std::clock_t>(-1)) {
      return std::chrono::milliseconds{0};
    }
    return std::chrono::milliseconds{(now - start) * 1000 / CLOCKS_PER_SEC};
  }

  static auto variable(const char* name) -> std::optional<std::string> {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
      return std::nullopt;
    }
    return std::string{value};
  }

  static auto configured_limit() -> std::chrono::milliseconds {
    const std::optional<std::string> text = variable("ARISA_TEST_BUDGET_MS");
    if (!text.has_value()) {
      return default_limit;
    }
    long long parsed = 0;
    const std::string_view view{*text};
    const auto [end, error] =
        std::from_chars(view.data(), view.data() + view.size(), parsed);
    if (error != std::errc{} || end != view.data() + view.size()) {
      return default_limit;
    }
    return std::chrono::milliseconds{parsed};
  }

  std::chrono::milliseconds limit_;
  std::optional<std::string> log_;
  std::clock_t started_{0};
  std::vector<std::string> over_;
};

}

auto main(int argc, char** argv) -> int {
  ::testing::InitGoogleTest(&argc, argv);
  ::testing::TestEventListeners& listeners =
      ::testing::UnitTest::GetInstance()->listeners();
  arisa::testing::TimeBudget budget;
  listeners.Append(&budget);
  const int result = RUN_ALL_TESTS();
  listeners.Release(&budget);
  if (result != 0) {
    return result;
  }
  return budget.exceeded() ? 1 : 0;
}
