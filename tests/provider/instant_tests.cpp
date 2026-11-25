#include <arisa/provider/clock/instant.hpp>

#include <gtest/gtest.h>

#include <chrono>

namespace arisa::provider {

using namespace std::chrono_literals;

TEST(InstantTest, ArithmeticAndOrdering) {
  const Instant start{10ms};
  const Instant later = start + 5ms;
  EXPECT_EQ(later - start, 5ms);
  EXPECT_LT(start, later);
  EXPECT_EQ(later - 5ms, start);
  EXPECT_EQ(later.since_origin(), 15ms);
}

}
