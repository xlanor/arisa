#include <arisa/provider/counted_scope.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <utility>

namespace arisa::provider {

TEST(CountedScopeTest, EntersOnFirstAndLeavesOnLast) {
  auto entered = std::make_shared<int>(0);
  auto left = std::make_shared<int>(0);
  CountedScope scope{[entered]() -> std::expected<void, ProviderError> {
                       ++*entered;
                       return {};
                     },
                     [left]() noexcept { ++*left; }};
  {
    auto outer = scope.acquire();
    ASSERT_TRUE(outer.has_value());
    {
      const auto inner = scope.acquire();
      ASSERT_TRUE(inner.has_value());
      EXPECT_EQ(scope.holders(), 2U);
    }
    EXPECT_EQ(*left, 0);
    const support::ScopedCallback moved{std::move(*outer)};
    EXPECT_EQ(scope.holders(), 1U);
  }
  EXPECT_EQ(*entered, 1);
  EXPECT_EQ(*left, 1);
  EXPECT_EQ(scope.holders(), 0U);
}

TEST(CountedScopeTest, FailedEnterIsReportedAndNotCounted) {
  auto left = std::make_shared<int>(0);
  CountedScope scope{[]() -> std::expected<void, ProviderError> {
                       return std::unexpected(ProviderError{
                           ProviderErrorCode::platform_failure, 0x1234U});
                     },
                     [left]() noexcept { ++*left; }};
  const auto held = scope.acquire();
  ASSERT_FALSE(held.has_value());
  EXPECT_EQ(held.error().native_result(), 0x1234U);
  EXPECT_EQ(scope.holders(), 0U);
  EXPECT_EQ(*left, 0);
}

TEST(CountedScopeTest, HolderMayOutliveTheScope) {
  auto left = std::make_shared<int>(0);
  std::optional<support::ScopedCallback> survivor;
  {
    CountedScope scope{
        []() -> std::expected<void, ProviderError> { return {}; },
        [left]() noexcept { ++*left; }};
    auto held = scope.acquire();
    ASSERT_TRUE(held.has_value());
    survivor.emplace(std::move(*held));
  }
  EXPECT_EQ(*left, 0);
  survivor.reset();
  EXPECT_EQ(*left, 1);
}

}
