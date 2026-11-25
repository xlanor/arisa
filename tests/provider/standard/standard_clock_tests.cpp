#include <arisa/provider/standard/standard_clock.hpp>

#include <gtest/gtest.h>

#include <chrono>

namespace arisa::provider::standard {

TEST(StandardClockTest, MonotonicAndWallAreSane) {
  const StandardClock clock;
  const Instant first = clock.now();
  const Instant second = clock.now();
  EXPECT_LE(first, second);
  constexpr std::chrono::sys_days earliest{std::chrono::year{2020} /
                                           std::chrono::January / 1};
  EXPECT_GT(clock.wall_now(), earliest);
}

}
